/*
   +----------------------------------------------------------------------+
   | PHP Version 7                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2016 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author:                                                              |
   +----------------------------------------------------------------------+
   */

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef INT64_MAX
#define INT64_MAX           INT64_C( 9223372036854775807)
#endif
#ifndef INT64_MIN
#define INT64_MIN         (-INT64_C( 9223372036854775807)-1)
#endif

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/compiler/parser.h>

#include "reader.h"
#include "writer.h"
#include "protobuf.h"

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_bdprotobuf.h"

using namespace std;

using namespace google::protobuf;

using namespace google::protobuf::io;

using namespace google::protobuf::compiler;

#define PB_FIELD_NAME "name"
#define PB_FIELD_REQUIRED "required"
#define PB_FIELD_REPEATED "repeated"
#define PB_FIELD_TYPE "type"
#define PB_FIELD_NUM "num"


static ulong PB_FIELD_TYPE_HASH;
static ulong PB_FIELD_NAME_HASH;
static ulong PB_FIELD_REQUIRED_HASH;
static ulong PB_FIELD_REPEATED_HASH;
static ulong PB_FIELD_NUM_HASH;

static int PB_FIELD_NAME_IDX = 0;
static int PB_FIELD_REQUIRED_IDX = 1;
static int PB_FIELD_REPEATED_IDX = 2;
static int PB_FIELD_TYPE_IDX = 3;
static int PB_FIELD_NUM_IDX = 4;

class CompilerErrorCollector : public MultiFileErrorCollector {
	public:
		void AddError(const string &filename, int line, int column, const string &message) {
			if (line == -1) {
				php_error(E_WARNING,"compile proto file error, file: %s, msg: %s", filename.c_str(), message.c_str());
			} else {
				php_error(E_WARNING,"compile proto file error, file: %s, line: %d, msg: %s", filename.c_str(), line+1, message.c_str());
			}
		}
};

/*
static int to_php_type[FieldDescriptor::MAX_TYPE + 1] = {0,
	FieldDescriptor::TYPE_DOUBLE,
	FieldDescriptor::TYPE_FLOAT,
	FieldDescriptor::TYPE_INT64,
	FieldDescriptor::TYPE_INT64,
	FieldDescriptor::TYPE_INT64,
	FieldDescriptor::TYPE_FIXED64,
	FieldDescriptor::TYPE_FIXED32,
	FieldDescriptor::TYPE_BOOL,
	FieldDescriptor::TYPE_STRING,
	FieldDescriptor::TYPE_GROUP,
	FieldDescriptor::TYPE_MESSAGE,
	FieldDescriptor::TYPE_BYTES,
	FieldDescriptor::TYPE_INT64,
	FieldDescriptor::TYPE_ENUM,
	FieldDescriptor::TYPE_FIXED32,
	FieldDescriptor::TYPE_FIXED64,
	FieldDescriptor::TYPE_SINT64,
	FieldDescriptor::TYPE_SINT64 
	};
*/
static char * const type_to_name[FieldDescriptor::MAX_TYPE + 1] = {
	"ERROR",     // 0 is reserved for errors

	"double",    // TYPE_DOUBLE
	"float",     // TYPE_FLOAT
	"int64",     // TYPE_INT64
	"uint64",    // TYPE_UINT64
	"int32",     // TYPE_INT32
	"fixed64",   // TYPE_FIXED64
	"fixed32",   // TYPE_FIXED32
	"bool",      // TYPE_BOOL
	"string",    // TYPE_STRING
	"group",     // TYPE_GROUP
	"message",   // TYPE_MESSAGE
	"bytes",     // TYPE_BYTES
	"uint32",    // TYPE_UINT32
	"enum",      // TYPE_ENUM
	"sfixed32",  // TYPE_SFIXED32
	"sfixed64",  // TYPE_SFIXED64
	"sint32",    // TYPE_SINT32
	"sint64",    // TYPE_SINT64
};


bool fileImport(zend_string * path, zend_string * filename, zval *parrent_array);

void buildFileArray(const FileDescriptor *filedesc,zval *parent_array);
void buildDescArray(const Descriptor *desc,zval *parent_array);


const char *pb_get_wire_type_name(int wire_type);
int pb_get_wire_type(int field_type);

bool unserialize_from_pb(reader_t *reader, HashTable * descriptors, zval * parrent_array,char * message_type, zval * unpack_method);
bool serialize_to_pb(writer_t *writer, HashTable * descriptors, zval *desc, HashTable *arr_input, char * message_type , zval * pack_method);
bool pb_unserialize_field_value(reader_t *reader, HashTable * descriptors, zval * parrent_array, bool repeated, char * field_key, zval *field_type, uint8_t wire_type, char * message_type, zval * unpack_method);
bool pb_serialize_field_value(writer_t *writer, HashTable * descriptors, zval *desc, uint32_t field_number, char *  field_key, zval *type, zval *value, char * message_type, zval * pack_method);

int pb_get_wire_type(int field_type)
{
	int ret;

	switch (field_type)
	{
		case FieldDescriptor::TYPE_SFIXED64:
		case FieldDescriptor::TYPE_DOUBLE:
		case FieldDescriptor::TYPE_FIXED64:
			ret = WIRE_TYPE_64BIT;
			break;

		case FieldDescriptor::TYPE_ENUM:
		case FieldDescriptor::TYPE_SFIXED32:
		case FieldDescriptor::TYPE_FIXED32:
		case FieldDescriptor::TYPE_FLOAT:
			ret = WIRE_TYPE_32BIT;
			break;

		case FieldDescriptor::TYPE_SINT32:
		case FieldDescriptor::TYPE_UINT32:
		case FieldDescriptor::TYPE_INT32:
		case FieldDescriptor::TYPE_UINT64:
		case FieldDescriptor::TYPE_INT64:
		case FieldDescriptor::TYPE_SINT64:
		case FieldDescriptor::TYPE_BOOL:
			ret = WIRE_TYPE_VARINT;
			break;

		case FieldDescriptor::TYPE_BYTES:
		case FieldDescriptor::TYPE_STRING:
			ret = WIRE_TYPE_LENGTH_DELIMITED;
			break;

		default:
			ret = -1;
			break;
	}

	return ret;
}

const char *pb_get_wire_type_name(int wire_type)
{
	const char *name;

	switch (wire_type)
	{
		case WIRE_TYPE_VARINT:
			name = "varint";
			break;

		case WIRE_TYPE_64BIT:
			name = "64bit";
			break;

		case WIRE_TYPE_LENGTH_DELIMITED:
			name = "length-delimited";
			break;

		case WIRE_TYPE_32BIT:
			name = "32bit";
			break;

		default:
			name = "unknown";
			break;
	}

	return name;
}

// read proto and build
bool fileImport(zend_string * path, zend_string * filename, zval * parent_array)
{
	DiskSourceTree * sourceTree = new DiskSourceTree();
	sourceTree->MapPath("", ZSTR_VAL(path));
	MultiFileErrorCollector *error_collector = new CompilerErrorCollector();

	Importer* importer = new Importer(sourceTree, error_collector);

	if (!importer){
		PHP_BDPB_WARNING("path invalid , line[%d] path[%s]" , __LINE__ ,ZSTR_VAL(path));
		delete sourceTree;
		delete error_collector;
		return false;
	}

	const FileDescriptor *filedesc = importer->Import(ZSTR_VAL(filename));

	if (!filedesc){
		PHP_BDPB_WARNING("file invalid , line[%d] filename[%s]", __LINE__ ,ZSTR_VAL(filename));
		delete sourceTree;
		delete importer;
		delete error_collector;
		return false;
	}

	buildFileArray(filedesc, parent_array);

	delete sourceTree;
	delete importer;
	delete error_collector;
	return true;
}

// build proto dependence and build msg desc
void buildFileArray(const FileDescriptor * filedesc, zval * parent_array)
{
	for(int j=0;j<filedesc->dependency_count(); j++){
		const FileDescriptor * dfd = filedesc->dependency(j);
		buildFileArray(dfd, parent_array);
	}

	for(int j=0; j<filedesc->message_type_count(); j++){
		const Descriptor *desc = filedesc->message_type(j);
		buildDescArray(desc, parent_array);
	}
}

// build msg desc
void buildDescArray(const Descriptor *desc,zval *parent_array)
{
	zval sub_array;
	array_init(&sub_array);

	for( int i=0; i<desc->field_count(); i++)
	{
		const FieldDescriptor * field = desc->field(i);
		zval field_array;
		array_init(&field_array);

		add_index_string( &field_array , PB_FIELD_NAME_IDX , const_cast<char*>( field->name().c_str()) ); 

		add_index_long( &field_array,PB_FIELD_REQUIRED_IDX, (field->label() == FieldDescriptor::LABEL_REQUIRED ? 1 : 0) );
		add_index_long( &field_array,PB_FIELD_REPEATED_IDX, (field->label() == FieldDescriptor::LABEL_REPEATED ? 1 : 0) );

		if( field->type() == FieldDescriptor::TYPE_MESSAGE ){
			add_index_string( &field_array,PB_FIELD_TYPE_IDX,const_cast<char*>((field->message_type()->full_name()).c_str()));
		}
		/*
		   else if( field->type() == FieldDescriptor::TYPE_ENUM ){
		   add_index_long(field_array,PB_FIELD_TYPE_IDX,(long)field->type());
		   }
		   */
		else{
			add_index_long( &field_array,PB_FIELD_TYPE_IDX,(long)field->type());
		}

		add_index_zval( &sub_array,(ulong)(field->number()), &field_array);
	}

	add_assoc_zval(parent_array, const_cast<char*>((desc->full_name()).c_str()), &sub_array);

	for( int j=0; j<desc->nested_type_count();j++){
		const Descriptor * nd = desc->nested_type(j);
		buildDescArray( nd, parent_array );
	}
}

bool unserialize_from_pb( reader_t *reader , HashTable *descriptors , zval * parrent_array , char * message_type,zval * unpack_method )
{
	char *field_key;
	HashTable * field_descriptor, *desc;
	bool repeated,required,is_last_repeated=false;

	zval *z_field_descriptor, *z_field_type, *z_field_key, *z_required, *z_repeated, *z_desc, *z_array;//, **z_value ,*repeated_array
	zval repeated_array;

	uint8_t wire_type;
	uint32_t field_number,last_field_num=0;
	int expected_wire_type, str_size, pack_size, subpack_size, ret;
	//if (zend_hash_find(descriptors, message_type, strlen(message_type)+1, (void **) &z_desc) == FAILURE){
	if( ( z_desc = zend_hash_find(descriptors , zend_string_init(message_type , strlen(message_type) , 0))) == NULL ){ 
		php_error(E_WARNING, "message_type:%s does not exist in file! line:%d", message_type, __LINE__);
		return false;
	}
	if(Z_TYPE_P(z_desc) != IS_ARRAY){
		php_error(E_WARNING, "message_type:%s does not exist in file! line:%d", message_type, __LINE__);
		return false;
	}
	array_init(parrent_array);
	desc = Z_ARRVAL_P(z_desc);
	while (reader_has_more(reader)) {
		if (reader_read_tag(reader, &field_number, &wire_type) != 0)
			break;
		if (!reader_has_more(reader))
			break;
		if(last_field_num != field_number){
			if(is_last_repeated){
				add_assoc_zval(parrent_array, field_key, &repeated_array);
			}
			//if (zend_hash_index_find(desc, field_number, (void **) &z_field_descriptor) == FAILURE) {
			if( (z_field_descriptor = zend_hash_index_find( desc , field_number)) == NULL ){ 
				switch (wire_type)
				{
					case WIRE_TYPE_VARINT:
						ret = reader_skip_varint(reader);
						break;

					case WIRE_TYPE_64BIT:
						ret = reader_skip_64bit(reader);
						break;

					case WIRE_TYPE_LENGTH_DELIMITED:
						ret = reader_skip_length_delimited(reader);
						break;

					case WIRE_TYPE_32BIT:
						ret = reader_skip_32bit(reader);
						break;

					default:
						php_error(E_WARNING,"unexpected wire type %ld for unexpected %u field", wire_type, field_number);
						return false;
				}

				if (ret != 0) {
					php_error(E_WARNING,"parse unexpected %u field of wire type %ld fail", field_number, wire_type);
					return false;
				}

				continue;
			}
			if(Z_TYPE_P(z_field_descriptor) != IS_ARRAY){
				php_error(E_WARNING,"descriptor of field:%u is invalid", field_number);
				continue;
			}
			field_descriptor = Z_ARRVAL_P(z_field_descriptor);

			//if (zend_hash_index_find(field_descriptor, PB_FIELD_TYPE_IDX, (void **) &z_field_type) == FAILURE){
			if( (z_field_type = zend_hash_index_find( field_descriptor , PB_FIELD_TYPE_IDX )) == NULL ){
				php_error(E_WARNING, "messagetype:%s is invalid, missing %s! line:%d",message_type,PB_FIELD_TYPE, __LINE__);
				continue;
			}

			//if (zend_hash_index_find(field_descriptor, PB_FIELD_NAME_IDX, (void **) &z_field_key) == FAILURE){
			if( (z_field_key = zend_hash_index_find( field_descriptor , PB_FIELD_NAME_IDX  )) == NULL ){
				php_error(E_WARNING, "messagetype:%s is invalid, missing %s! line:%d",message_type,PB_FIELD_NAME, __LINE__);
				continue;
			}
			field_key = Z_STRVAL_P(z_field_key);

			repeated = false;

			//if (zend_hash_index_find(field_descriptor, PB_FIELD_REPEATED_IDX, (void **) &z_repeated) == FAILURE){
			if( (z_repeated = zend_hash_index_find( field_descriptor , PB_FIELD_REPEATED_IDX )) == NULL ){
				php_error(E_WARNING, "messagetype:%s is invalid,missing %s or %s! line:%d",message_type,PB_FIELD_REQUIRED,PB_FIELD_REPEATED, __LINE__);
			}else{
				//repeated = Z_BVAL_P(z_repeated);
				//qmy 
				repeated = (unsigned char)(Z_LVAL_P(z_repeated));
				/*
				php_error(E_WARNING, "messagetype:%s is invalid,missing %s or %s! line:%d",message_type,PB_FIELD_REQUIRED,PB_FIELD_REPEATED, Z_TYPE_P(z_required) );
				if( Z_TYPE_P(z_required) == IS_TRUE ){
					repeated = true;
				}
				else{
					repeated = false;
				}
				*/
			}
		}
		bool succ = false;
		if(repeated && (last_field_num == field_number)){
			succ = pb_unserialize_field_value(reader, descriptors, &repeated_array, repeated, field_key, z_field_type, wire_type, message_type, unpack_method);
		}
		else if(repeated){
			//ALLOC_INIT_ZVAL(repeated_array);
			array_init(&repeated_array);
			succ = pb_unserialize_field_value(reader, descriptors, &repeated_array, repeated, field_key, z_field_type, wire_type, message_type, unpack_method);
		}
		else{
			succ = pb_unserialize_field_value(reader, descriptors, parrent_array, repeated, field_key, z_field_type, wire_type, message_type, unpack_method);
		}
		last_field_num = field_number;
		is_last_repeated = repeated;

		if(!succ){
			if(required){
				return false;
			}
			else{
				continue;
			}
		}
	}
	if(is_last_repeated){
		add_assoc_zval(parrent_array, field_key, &repeated_array);
	}
	return true;

}

bool serialize_to_pb( writer_t *writer , HashTable *descriptors , zval * z_desc, HashTable *arr_input , char * message_type , zval * pack_method )
{

	HashTable *desc,*field_descriptor;
	HashPosition i, j;
	zend_string *field_key;
	ulong field_number;
	zval *z_field_descriptor, *z_field_type, *z_field_key, *z_required, *z_repeated, *z_value, *z_array, *sub_field_descriptor;
	bool repeated,required;

	if(Z_TYPE_P(z_desc) != IS_ARRAY){
		php_error(E_WARNING, "message_type:%s does not exist in file! line:%d", message_type, __LINE__);
		return false;
	}
	desc = Z_ARRVAL_P(z_desc);

	PB_FOREACH(&i, desc) {
		z_value = NULL;
		repeated = false;
		required = false;
		//zend_hash_get_current_key_ex(desc, NULL, NULL, &field_number, 0, &i);
		zend_hash_get_current_key_ex(desc , NULL , &field_number , &i);
		//zend_hash_get_current_data_ex(desc, (void **) &z_field_descriptor, &i);
		z_field_descriptor = zend_hash_get_current_data_ex(desc , &i);
		if(Z_TYPE_P(z_field_descriptor) != IS_ARRAY){
			php_error(E_WARNING, "message_type:%s is invalid! line:%d", message_type, __LINE__);
			continue;
		}
		field_descriptor = Z_ARRVAL_P(z_field_descriptor);

		//if (zend_hash_index_find(field_descriptor, PB_FIELD_NAME_IDX, (void **) &z_field_key) == FAILURE){
		if( (z_field_key = zend_hash_index_find( field_descriptor , PB_FIELD_NAME_IDX) ) == NULL ){
			php_error(E_WARNING, "messagetype:%s is invalid, missing %s! line:%d",message_type,PB_FIELD_NAME, __LINE__);
			continue;
		}
		//field_key = Z_STRVAL_P(z_field_key);
		char * tmp_field_key = Z_STRVAL_P(z_field_key);
		field_key = zend_string_init( tmp_field_key , strlen(tmp_field_key) , 0 );

		//if (zend_hash_index_find(field_descriptor, PB_FIELD_REQUIRED_IDX, (void **) &z_required) != FAILURE){
		if( (z_required = zend_hash_index_find(field_descriptor , PB_FIELD_REQUIRED_IDX) ) != NULL ){
			//required = Z_BVAL_P(z_required);
			required = (unsigned char)(Z_LVAL_P(z_required));
		}

		//if (zend_hash_find(arr_input, field_key, strlen(field_key)+1, (void **) &z_value) == FAILURE){
		if( (z_value = zend_hash_find(arr_input , field_key) ) == NULL ){
			if(required){
				php_error(E_WARNING, "in input key:%s is required and must be set! line:%d",field_key, __LINE__);
				return false;
			}else{
				continue;
			}
		}

		if(Z_TYPE_P(z_value) == IS_NULL){
			if(required){
				php_error(E_WARNING, "in input key:%s is required and must be set! line:%d",field_key, __LINE__);
				return false;
			}else{
				continue;
			}
		}

		if(!required){
			//if (zend_hash_index_find(field_descriptor, PB_FIELD_REPEATED_IDX, (void **) &z_repeated) == FAILURE){
			if( ( z_repeated = zend_hash_index_find( field_descriptor , PB_FIELD_REPEATED_IDX) ) == NULL ){
				php_error(E_WARNING, "messagetype:%s is invalid, missing %s or %s! line:%d",message_type,PB_FIELD_REQUIRED,PB_FIELD_REPEATED, __LINE__);
				continue;
			}else{
				//repeated = Z_BVAL_P(z_repeated);
				repeated = (unsigned char)(Z_LVAL_P(z_repeated));
			}
		}

		//if (zend_hash_index_find(field_descriptor, PB_FIELD_TYPE_IDX, (void **) &z_field_type) == FAILURE){
		if( ( z_field_type = zend_hash_index_find(field_descriptor , PB_FIELD_TYPE_IDX) ) == NULL ){
			php_error(E_WARNING, "messagetype:%s is invalid, missing %s! line:%d",message_type, PB_FIELD_TYPE, __LINE__);
			continue;
		}

		if((Z_TYPE_P(z_field_type) == IS_STRING) && (Z_TYPE_P(z_value) == IS_ARRAY)){
			char * tmp_field_key = Z_STRVAL_P(z_field_type);
			zend_string * sub_field_key = zend_string_init( tmp_field_key , strlen(tmp_field_key) , 0 ); 
			//if(zend_hash_find(descriptors, sub_field_key, strlen(sub_field_key)+1, (void **) &sub_field_descriptor) == FAILURE){
			if( (sub_field_descriptor = zend_hash_find(descriptors , sub_field_key) ) == NULL ){
				if(required){
					php_error(E_WARNING, "message_type:%s does not exist in file! line:%d", sub_field_key, __LINE__);
					return false;
				}
			}
		}


		if(repeated && (Z_TYPE_P(z_value) == IS_ARRAY)){
			z_array = z_value;
			PB_FOREACH(&j, Z_ARRVAL_P(z_array)) {
				//zend_hash_get_current_data_ex(Z_ARRVAL_P(z_array), (void **) &z_value, &j);
				z_value = zend_hash_get_current_data_ex( Z_ARRVAL_P(z_array) , &j);
				if(!pb_serialize_field_value(writer, descriptors,sub_field_descriptor, field_number, ZSTR_VAL(field_key), z_field_type, z_value, message_type, pack_method)){

					continue;
				}
			}
		}else{
			if(!pb_serialize_field_value(writer, descriptors,sub_field_descriptor, field_number, ZSTR_VAL(field_key), z_field_type, z_value, message_type, pack_method)){
				if(required){
					return false;
				}else{
					continue;
				}
			}
		}

	}
	return true;
}


bool pb_unserialize_field_value(reader_t *reader, HashTable * descriptors, zval * parrent_array, bool repeated, char * field_key, zval *field_type, uint8_t wire_type, char * message_type, zval * unpack_method)
{
	int expected_wire_type, str_size, ret, subpack_size;
	char *str, *subpack;
	//zval *value;
	zval value;
	//qmy
	if (Z_TYPE_P(field_type) == IS_STRING) {
		if (wire_type != WIRE_TYPE_LENGTH_DELIMITED) {
			php_error(E_WARNING, "'%s' field wire type is %s but should be string", field_key, pb_get_wire_type_name(wire_type));
			return false;
		}
		if ((ret = reader_read_string(reader, &subpack, &subpack_size)) == 0){
			reader_t sub_reader;
			reader_init(&sub_reader, subpack, subpack_size);
			//ALLOC_INIT_ZVAL(value);
			bool success = unserialize_from_pb(&sub_reader,descriptors, &value,Z_STRVAL_P(field_type) ,unpack_method);
			if(success){
				if(repeated){
					add_next_index_zval(parrent_array, &value);
				}
				else{
					add_assoc_zval(parrent_array, field_key, &value);
				}
			}else{
				zval_dtor(&value);
			}
			return success;
		}

	}
	else if (Z_TYPE_P(field_type) == IS_LONG){
		if ((expected_wire_type = pb_get_wire_type(Z_LVAL_P(field_type))) != wire_type) {
			php_error(E_WARNING, "type:%s of field:%s in message_type:%s does not match the type:%s in input! line:%d",type_to_name[Z_LVAL_P(field_type)], field_key, message_type, pb_get_wire_type_name(wire_type), __LINE__);
			return false;
		}
		switch (Z_LVAL_P(field_type))
		{
			case FieldDescriptor::TYPE_DOUBLE:
				{
					double out = 0.0;
					ret = reader_read_double(reader, &out);
					if(repeated){
						add_next_index_double(parrent_array,out);
					}
					else{
						add_assoc_double(parrent_array, field_key, out);
					}
				}
				break;

			case FieldDescriptor::TYPE_FIXED32:
				{
					int64_t out = 0;
					ret = reader_read_fixed32(reader, &out);
					if(repeated){
						add_next_index_long(parrent_array,(uint32_t)out);
					}
					else{
						add_assoc_long(parrent_array, field_key, (uint32_t)out);
					}
				}
				break;

			case FieldDescriptor::TYPE_FIXED64:
				{
					int64_t out = 0;
					ret = reader_read_fixed64(reader, &out);
					if((uint64_t)out>LONG_MAX){
						if(repeated){
							add_next_index_double(parrent_array, (uint64_t)out);
						}
						else{
							add_assoc_double(parrent_array, field_key, (uint64_t)out);
						}
					}else{
						if(repeated){
							add_next_index_long(parrent_array, (uint64_t)out);
						}
						else{
							add_assoc_long(parrent_array, field_key, (uint64_t)out);
						}
					}
				}
				break;

			case FieldDescriptor::TYPE_FLOAT:
				{
					double out = 0.0;
					ret = reader_read_float(reader, &out);
					if(repeated){
						add_next_index_double(parrent_array, (float)out);
					}
					else{
						add_assoc_double(parrent_array, field_key, (float)out);
					}
				}
				break;

			case FieldDescriptor::TYPE_INT64:
				{
					int64_t out = 0;
					ret = reader_read_int(reader, &out);
					if(repeated){
						add_next_index_long(parrent_array, out);
					}
					else{
						add_assoc_long(parrent_array, field_key, out);
					}
				}
				break;

			case FieldDescriptor::TYPE_BOOL:
				{
					int64_t out = 0;
					ret = reader_read_int(reader, &out);
					if(repeated){
						add_next_index_bool(parrent_array, out);
					}
					else{
						add_assoc_bool(parrent_array, field_key, out);
					}
				}
				break;

			case FieldDescriptor::TYPE_SINT64:
				{
					int64_t out = 0;
					ret = reader_read_signed_int(reader, &out);
					if(repeated){
						add_next_index_long(parrent_array, out);
					}
					else{
						add_assoc_long(parrent_array, field_key, out);
					}
				}
				break;

			case FieldDescriptor::TYPE_SFIXED64:
				{
					int64_t out = 0;
					ret = reader_read_fixed64(reader, &out);
					if(repeated){
						add_next_index_long(parrent_array, out);
					}
					else{
						add_assoc_long(parrent_array, field_key, out);
					}
				}
				break;

			case FieldDescriptor::TYPE_SFIXED32:
				{
					int64_t out = 0;
					ret = reader_read_fixed32(reader, &out);
					if(repeated){
						add_next_index_long(parrent_array,(int32_t)out);
					}
					else{
						add_assoc_long(parrent_array, field_key, (int32_t)out);
					}
				}
				break;

			case FieldDescriptor::TYPE_SINT32:
				{
					int64_t out = 0;
					ret = reader_read_signed_int(reader, &out);
					if(repeated){
						add_next_index_long(parrent_array,(int32_t)out);
					}
					else{
						add_assoc_long(parrent_array, field_key, (int32_t)out);
					}
				}
				break;

			case FieldDescriptor::TYPE_UINT32:
				{
					int64_t out = 0;
					ret = reader_read_int(reader, &out);
					if(repeated){
						add_next_index_long(parrent_array,(uint32_t)out);
					}
					else{
						add_assoc_long(parrent_array, field_key, (uint32_t)out);
					}
				}
				break;

			case FieldDescriptor::TYPE_ENUM:
			case FieldDescriptor::TYPE_INT32:
				{
					int64_t out = 0;
					ret = reader_read_int(reader, &out);
					if(repeated){
						add_next_index_long(parrent_array,(int32_t)out);
					}
					else{
						add_assoc_long(parrent_array, field_key, (int32_t)out);
					}
				}
				break;

			case FieldDescriptor::TYPE_UINT64:
				{
					int64_t out = 0;
					ret = reader_read_int(reader, &out);
					if(repeated){
						if((uint64_t)out>LONG_MAX){
							add_next_index_double(parrent_array,(uint64_t)out);
						}else{
							add_next_index_long(parrent_array,(uint64_t)out);
						}
					}
					else{
						if((uint64_t)out>LONG_MAX){
							add_assoc_double(parrent_array, field_key, (uint64_t)out);
						}else{
							add_assoc_long(parrent_array, field_key, (uint64_t)out);
						}
					}
				}
				break;

			case FieldDescriptor::TYPE_STRING:
				{
					if ((ret = reader_read_string(reader, &str, &str_size)) == 0){
						//ALLOC_INIT_ZVAL(value);
						ZVAL_STRINGL(&value, str, str_size);
						if(repeated){
							add_next_index_zval(parrent_array,&value);
						}
						else{
							add_assoc_zval(parrent_array, field_key, &value);
						}
					}
				}
				break;

			case FieldDescriptor::TYPE_BYTES:
				{
					if ((ret = reader_read_string(reader, &str, &str_size)) == 0){
						if(unpack_method == NULL){
							//ALLOC_INIT_ZVAL(value);
							ZVAL_STRINGL(&value, str, str_size);
							if(repeated){
								add_next_index_zval(parrent_array,&value);
							}
							else{
								add_assoc_zval(parrent_array, field_key, &value);
							}
						}
						else{
							zval args[1];
							zval  arg;
							//ALLOC_INIT_ZVAL(value);
							zval_dtor(&value);
							//INIT_ZVAL(arg);
							ZVAL_STRINGL(&arg, str, str_size);
							Z_ADDREF(arg);
							args[0] = arg;
							if (call_user_function(EG(function_table), NULL, unpack_method, &value, 1, args TSRMLS_CC) == SUCCESS) {
							//if (call_user_function(EG(function_table), NULL, unpack_method, &value, 1, &args TSRMLS_CC) == SUCCESS) {
								if(repeated){
									add_next_index_zval(parrent_array, &value);
								}
								else{
									add_assoc_zval(parrent_array, field_key, &value);
								}
								zval_dtor(&arg);
							}
							else{
								zval_dtor(&arg);
								php_error(E_WARNING, "call unpack_method failed, field:%s of message_type:%s! line:%d",field_key,message_type, __LINE__);
								return false;
							}

						}
					}
					else{
						php_error(E_WARNING, "field:%s of message_type:%s is type of bytes and in input pack it is empty! line:%d", field_key, message_type, __LINE__);
						return false;
					}
				}

				break;

			default:
				php_error(E_WARNING, "type of field:%s in message_type:%s is not supported! line:%d", field_key, message_type, __LINE__);
				return false;
		}
	}
	else{
		php_error(E_WARNING, "type of desc info of field:%s in message_type:%s is invalid! line:%d", field_key, message_type, __LINE__);
		return false;
	}
	return true;
}


bool pb_serialize_field_value(writer_t *writer, HashTable * descriptors, zval *desc, uint32_t field_number,char *  field_key, zval *type, zval *value, char *message_type, zval *pack_method)
{
	int r;
	if (Z_TYPE_P(type) == IS_STRING) {
		if(Z_TYPE_P(value) == IS_ARRAY){
			writer_t msg_writer;
			char *pack;
			int pack_size;
			writer_init(&msg_writer);
			bool success = serialize_to_pb(&msg_writer, descriptors, desc, Z_ARRVAL_P(value) , Z_STRVAL_P(type) , pack_method);
			if(success){
				pack = writer_get_pack(&msg_writer, &pack_size);
				if(pack && pack_size > 0){
					writer_write_message(writer, field_number, pack, pack_size);
				}
				writer_free_pack(&msg_writer);
			}
			else{
				writer_free_pack(&msg_writer);
				return false;
			}
		}
		else{
			php_error(E_WARNING, "field:%s of message_type:%s is type of message and in input reserved key type must be array! line:%d", field_key, message_type, __LINE__);
			return false;
		}

	}
	else if(Z_TYPE_P(type) == IS_LONG){
		if((Z_TYPE_P(value) == IS_ARRAY || Z_TYPE_P(value) == IS_OBJECT) && Z_LVAL_P(type) !=FieldDescriptor::TYPE_BYTES){
			php_error(E_WARNING, "field:%s of message_type:%s is not type of bytes and in input type cannot be array! line:%d", field_key, message_type, __LINE__);
			return false;
		}
		switch (Z_LVAL_P(type))
		{
			case FieldDescriptor::TYPE_BYTES:
				{
					if(pack_method == NULL){
						if(Z_TYPE_P(value) == IS_STRING){
							r = writer_write_string(writer, field_number, Z_STRVAL_P(value), Z_STRLEN_P(value));
						}
						else if(!INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_ARRAY && Z_TYPE_P(value) != IS_OBJECT){
							zval expr_copy;
							int use_copy;
							use_copy = zend_make_printable_zval(value, &expr_copy);
							//zend_make_printable_zval(value, &expr_copy, &use_copy);
							//zend_make_printable_zval(*value, &expr_copy, &use_copy);
							if(use_copy){
								r = writer_write_string(writer, field_number, Z_STRVAL(expr_copy), Z_STRLEN(expr_copy));
							}
							else{
								r = writer_write_string(writer, field_number, Z_STRVAL_P(value), Z_STRLEN_P(value));
							}
							zval_dtor(&expr_copy);
						}
						else{
							php_error(E_WARNING, "field:%s of message_type:%s is type of bytes and in input pack_method cannot be NULL! line:%d", field_key, message_type, __LINE__);
							return false;
						}
					}
					else{
						zval argvs[1];
						zval retval;
						argvs[0] = *value;
						//argvs[0] = *value;
						if (call_user_function(EG(function_table), NULL, pack_method, &retval, 1, argvs) == SUCCESS) {
						//if (call_user_function(EG(function_table), NULL, pack_method, &retval, 1, argvs) == SUCCESS) {
							if (Z_TYPE(retval) != IS_STRING){
								php_error(E_WARNING, "retval of pack_method is not string! field:%s of message_type:%s. line:%d", field_key,message_type,__LINE__);
								return false;
							}
							if (Z_STRLEN(retval) > 0){
								writer_write_message(writer, field_number, Z_STRVAL(retval), Z_STRLEN(retval));
							}
							zval_dtor(&retval);
						}
						else{
							php_error(E_WARNING, "call pack_method failed!field:%s of message_type:%s. line:%d", field_key,message_type,__LINE__);
							return false;
						}
					}
				}
				break;

			case FieldDescriptor::TYPE_DOUBLE:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not double in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_double(writer, field_number, Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_double(value);
						//convert_to_double(*value);
						r = writer_write_double(writer, field_number, Z_DVAL_P(value));
					}
					else
					{
						r = writer_write_double(writer, field_number, (double)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_FLOAT:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not float in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_float(writer, field_number, (float)Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_double(value);
						//convert_to_double(*value);
						r = writer_write_float(writer, field_number, (float)Z_DVAL_P(value));
					}
					else{
						r = writer_write_float(writer, field_number, (float)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_BOOL:
			case FieldDescriptor::TYPE_INT64:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not long in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_int(writer, field_number, (int64_t)Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_int(writer, field_number, Z_LVAL_P(value));
					}
					else{
						r = writer_write_int(writer, field_number, Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_FIXED32:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not fixed32 in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_fixed32(writer, field_number, (uint32_t)Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_fixed32(writer, field_number, (uint32_t)Z_LVAL_P(value));
					}
					else{
						r = writer_write_fixed32(writer, field_number, (uint32_t)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_FIXED64:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not fixed64 in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_fixed64(writer, field_number, (uint64_t)Z_DVAL_P(value));
					}else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_fixed64(writer, field_number, (uint64_t)Z_LVAL_P(value));
					}
					else{
						r = writer_write_fixed64(writer, field_number, (uint64_t)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_STRING:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_STRING){
						php_error(E_WARNING, "type of field:%s is not string in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_STRING){
						r = writer_write_string(writer, field_number, Z_STRVAL_P(value), Z_STRLEN_P(value));
					}
					else{
						zval expr_copy;
						int use_copy;
						use_copy = zend_make_printable_zval(value, &expr_copy);
						//zend_make_printable_zval(*value, &expr_copy, &use_copy);
						if(use_copy){
							r = writer_write_string(writer, field_number, Z_STRVAL(expr_copy), Z_STRLEN(expr_copy));
						}
						else{
							r = writer_write_string(writer, field_number, Z_STRVAL_P(value), Z_STRLEN_P(value));
						}
						zval_dtor(&expr_copy);
					}
				}
				break;

			case FieldDescriptor::TYPE_SINT64:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not sint64 in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_signed_int(writer, field_number, (int64_t)Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_signed_int(writer, field_number, (int64_t)Z_LVAL_P(value));
					}
					else{
						r = writer_write_signed_int(writer, field_number, (int64_t)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_SFIXED64:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not sfixed64 in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_fixed64(writer, field_number, (int64_t)Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_fixed64(writer, field_number, (int64_t)Z_LVAL_P(value));
					}
					else{
						r = writer_write_fixed64(writer, field_number, (int64_t)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_SFIXED32:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not sfixed32 in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_fixed32(writer, field_number, (int32_t)Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_fixed32(writer, field_number, (int32_t)Z_LVAL_P(value));
					}
					else{
						r = writer_write_fixed32(writer, field_number, (int32_t)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_SINT32:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not sint32 in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_signed_int(writer, field_number, (int32_t)Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_signed_int(writer, field_number, (int32_t)Z_LVAL_P(value));
					}
					else{
						r = writer_write_signed_int(writer, field_number, (int32_t)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_UINT32:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not uint32 in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_int(writer, field_number, (uint32_t)Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_int(writer, field_number, (uint32_t)Z_LVAL_P(value));
					}
					else{
						r = writer_write_int(writer, field_number, (uint32_t)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_ENUM:
			case FieldDescriptor::TYPE_INT32:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not int32 in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_int(writer, field_number, (int32_t)Z_DVAL_P(value));
					}else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_int(writer, field_number, (int32_t)Z_LVAL_P(value));
					}else{
						r = writer_write_int(writer, field_number, (int32_t)Z_LVAL_P(value));
					}
				}
				break;

			case FieldDescriptor::TYPE_UINT64:
				{
					if(INI_INT("use_cpp_type") && Z_TYPE_P(value) != IS_DOUBLE && Z_TYPE_P(value) != IS_LONG && Z_TYPE_P(value) != _IS_BOOL){
						php_error(E_WARNING, "type of field:%s is not uint64 in message_type:%s! line:%d", field_key, message_type, __LINE__);
						return false;
					}
					if(Z_TYPE_P(value) == IS_DOUBLE){
						r = writer_write_int(writer, field_number, (uint64_t)Z_DVAL_P(value));
					}
					else if(Z_TYPE_P(value) == IS_STRING){
						convert_to_long_base(value, 10);
						//convert_to_long_base(*value, 10);
						r = writer_write_int(writer, field_number, (uint64_t)Z_LVAL_P(value));
					}
					else{
						r = writer_write_int(writer, field_number, (uint64_t)Z_LVAL_P(value));
					}
				}
				break;

			default:
				php_error(E_WARNING, "type of field:%s in message_type:%s is not supported! line:%d", field_key, message_type, __LINE__);
				return false;
		}
	}
	else{
		php_error(E_WARNING, "type of desc info of field:%s in message_type:%s is invalid! line:%d", field_key, message_type, __LINE__);
		return false;
	}

	return true;
}


/* If you declare any globals in php_bdprotobuf.h uncomment this:
   ZEND_DECLARE_MODULE_GLOBALS(bdprotobuf)
   */

/* True global resources - no need for thread safety here */
static int le_bdprotobuf;

/* {{{ PHP_INI
*/
PHP_INI_BEGIN()
	//STD_PHP_INI_ENTRY("bdprotobuf.use_cpp_type", "0", PHP_INI_ALL, OnUpdateString, global_string, zend_bdprotobuf_globals, bdprotobuf_globals)

	//qmy?
PHP_INI_END()
	/* }}} */

	/* Remove the following function when you have successfully modified config.m4
	   so that your module can be compiled into PHP, it exists only for testing
	   purposes. */

	/* Every user-visible function in PHP should document itself in the source */
	/* {{{ proto string confirm_bdprotobuf_compiled(string arg)
	   Return a string to confirm that the module is compiled in */
/*
PHP_FUNCTION(confirm_bdprotobuf_compiled)
{
	char *arg = NULL;
	size_t arg_len, len;
	zend_string *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	strg = strpprintf(0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "bdprotobuf", arg);

	RETURN_STR(strg);
}
*/
PHP_FUNCTION(proto2desc)
{
	zend_string * path, * filename;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "SS", &path, &filename) == FAILURE ){
		RETURN_FALSE;
	}

	if (ZSTR_LEN(path) <= 0
			|| ZSTR_LEN(filename) <= 0
	   ){
		RETURN_FALSE;
	}

	array_init(return_value);
	bool fd = fileImport(path, filename, return_value);
	if ( !fd ){
		RETURN_FALSE;
	}
}

PHP_FUNCTION(array2protobuf)
{
	zend_string * path , * filename , * pack_type , * pack_method;
	zval *param , pz_pack_method , descriptors;
	HashTable * descs , * arr_input = NULL;

	if( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC , "SSaS|S" , &path , &filename , &param , &pack_type , &pack_method) == FAILURE ){
		RETURN_FALSE;
	}

	if( Z_TYPE_P(param) != IS_ARRAY 
		|| ZSTR_LEN(path) <= 0
		|| ZSTR_LEN(filename) <= 0
	){
		RETURN_FALSE;
	}

	array_init(&descriptors);
	bool fd = fileImport(path , filename ,&descriptors);
	if(!fd){
		RETURN_FALSE;
	}

	arr_input = Z_ARRVAL_P(param);
	descs = Z_ARRVAL(descriptors);

	writer_t writer;
	writer_init(&writer);

	if(ZSTR_LEN(pack_method) > 0 ){
		ZVAL_STRING(&pz_pack_method , ZSTR_VAL(pack_method));
	}

	zval * desc;

	if( ( desc = zend_hash_find(descs , pack_type )) == NULL ){
	php_error(E_WARNING, "message_type:%s does not exist in file! line:%d", ZSTR_VAL(pack_type), __LINE__);
		RETURN_FALSE;
	}

	bool suc = serialize_to_pb( &writer , descs , desc , arr_input , ZSTR_VAL(pack_type) ,&pz_pack_method );
	char * pack;
	int pack_len;
	
	if(suc)
	{
		pack = writer_get_pack(&writer , &pack_len ) ;
		ZVAL_STRINGL(return_value , pack , pack_len );
		writer_free_pack(&writer);
	}
	else
	{
		writer_free_pack(&writer);
		RETURN_FALSE;
	}
}

PHP_FUNCTION(protobuf2array)
{
	zend_string * path , * filename , * pack_type , * input , * unpack_method;
	zval pz_unpack_method , descriptors;
	HashTable * descs;

	if( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC , "SSSS|S" , &path , &filename , &input , &pack_type , &unpack_method ) ){
		RETURN_FALSE;
	}

	if( ZSTR_LEN(path) <= 0
		|| ZSTR_LEN(filename) <= 0
		|| ZSTR_LEN(input) <= 0
	){
		RETURN_FALSE;
	}

	array_init(&descriptors);
	bool fd = fileImport(path , filename , &descriptors);
	if(!fd){
		RETURN_FALSE;
	}

	descs = Z_ARRVAL(descriptors);

	reader_t reader;
	reader_init(&reader , ZSTR_VAL(input), ZSTR_LEN(input) );

	if(ZSTR_LEN(unpack_method) > 0 ){
		ZVAL_STRING(&pz_unpack_method , ZSTR_VAL(unpack_method));
	}

	if(!unserialize_from_pb( &reader , descs , return_value , ZSTR_VAL(pack_type) ,&pz_unpack_method)){
		RETURN_FALSE;
	}
}

PHP_FUNCTION(array2protobufbydesc)
{
	zend_string * pack_type , * pack_method;
	zval * descriptors , * param ,  pz_pack_method; 

	HashTable * descs, *arr_input;

	if( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC , "aaS|S" , &descriptors , &param , &pack_type , &pack_method ) == FAILURE ){
		RETURN_FALSE;
	}

	if( Z_TYPE_P(param) != IS_ARRAY ){
		RETURN_FALSE;
	}

	arr_input = Z_ARRVAL_P(param);
	descs = Z_ARRVAL_P(descriptors);
	/*
	zval tmp;
	array_init(&tmp);
	ZVAL_COPY_VALUE(&tmp , descriptors);
	descs = Z_ARRVAL(tmp);
	*/
	//RETURN_ARR(arr_input);
	//RETURN_ARR(descs);
	zval * desc;

	if( (desc = zend_hash_find( descs , pack_type)) == NULL ){
		PHP_BDPB_WARNING("message_type does not exist in file , line[%d] message[%s]" , __LINE__ ,ZSTR_VAL(pack_type));
		RETURN_FALSE;
	}

	if( ZSTR_LEN(pack_method) > 0){
		ZVAL_STRING(&pz_pack_method , ZSTR_VAL(pack_method));
	}

	writer_t writer;
	writer_init(&writer);

	bool suc = serialize_to_pb( &writer , descs , desc , arr_input , ZSTR_VAL(pack_type) , &pz_pack_method );
	char * pack;
	int pack_size;

	if( suc ){
		pack = writer_get_pack(&writer, &pack_size);
		ZVAL_STRINGL(return_value ,pack , pack_size);
		writer_free_pack(&writer);
	}
	else{
		writer_free_pack(&writer);
		RETURN_FALSE;
	}
}

PHP_FUNCTION(protobuf2arraybydesc)
{
	zend_string * input , * pack_type , * unpack_method;
	zval * descriptors , pz_unpack_method;

	if( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC , "aSS|S" ,&descriptors ,&input , &pack_type , &unpack_method ) == FAILURE ){
		RETURN_FALSE;
	}

	if(ZSTR_LEN(input) <= 0 ){
		RETURN_FALSE;
	}

	HashTable * descs;
	descs = Z_ARRVAL_P(descriptors);

	reader_t reader;
	reader_init(&reader , ZSTR_VAL(input) , ZSTR_LEN(input) );

	if(ZSTR_LEN(unpack_method) > 0){
		ZVAL_STRING(&pz_unpack_method , ZSTR_VAL(unpack_method) );
	}

	if(!unserialize_from_pb( &reader , descs , return_value , ZSTR_VAL(pack_type) , &pz_unpack_method)){
		RETURN_FALSE;
	}
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
   */


/* {{{ php_bdprotobuf_init_globals
*/
/* Uncomment this function if you have INI entries
   static void php_bdprotobuf_init_globals(zend_bdprotobuf_globals *bdprotobuf_globals)
   {
   bdprotobuf_globals->global_value = 0;
   bdprotobuf_globals->global_string = NULL;
   }
   */
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
*/
PHP_MINIT_FUNCTION(bdprotobuf)
{

	REGISTER_INI_ENTRIES();

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
*/
PHP_MSHUTDOWN_FUNCTION(bdprotobuf)
{

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
*/
PHP_RINIT_FUNCTION(bdprotobuf)
{
#if defined(COMPILE_DL_BDPROTOBUF) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
*/
PHP_RSHUTDOWN_FUNCTION(bdprotobuf)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
*/
PHP_MINFO_FUNCTION(bdprotobuf)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "bdprotobuf support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	   DISPLAY_INI_ENTRIES();
	   */
}
/* }}} */

/* {{{ bdprotobuf_functions[]
 *
 * Every user visible function must have an entry in bdprotobuf_functions[].
 */
const zend_function_entry bdprotobuf_functions[] = {
	//PHP_FE(confirm_bdprotobuf_compiled,	NULL)		/* For testing, remove later. */
		PHP_FE(proto2desc,  NULL)
		PHP_FE(array2protobuf,  NULL)
		PHP_FE(protobuf2array,  NULL)
		PHP_FE(array2protobufbydesc,  NULL)
		PHP_FE(protobuf2arraybydesc,  NULL)
		PHP_FE_END	/* Must be the last line in bdprotobuf_functions[] */
};
/* }}} */

/* {{{ bdprotobuf_module_entry
*/
zend_module_entry bdprotobuf_module_entry = {
	STANDARD_MODULE_HEADER,
	"bdprotobuf",
	bdprotobuf_functions,
	PHP_MINIT(bdprotobuf),
	PHP_MSHUTDOWN(bdprotobuf),
	PHP_RINIT(bdprotobuf),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(bdprotobuf),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(bdprotobuf),
	PHP_BDPROTOBUF_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_BDPROTOBUF
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(bdprotobuf)
#endif

	/*
	 * Local variables:
	 * tab-width: 4
	 * c-basic-offset: 4
	 * End:
	 * vim600: noet sw=4 ts=4 fdm=marker
	 * vim<600: noet sw=4 ts=4
	 */
