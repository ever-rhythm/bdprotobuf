<?php


/*
 string(32) "ca9bc3c89b90935f245d9633a757c3d7"
 string(32) "60b56bc7630f74a153a344a1937b4cdf"
 string(32) "3e2688239576610c6f8a627ee2bf1538"
 string(5) "string(32) "ca9bc3c89b90935f245d9633a757c3d7"
 string(32) "3e2688239576610c6f8a627ee2bf1538"
 */


$msg = 'MgetUserDataReq';
$pack = 'msgpack_pack';
$unpack = 'msgpack_unpack';

$filePath = dirname(__FILE__);
$fileProto = 'mgetUserData.proto';

$arrIn = array(
	'user_id' => array(
		5,
		1496,
	),
);

$strOut = array2protobuf( $filePath , $fileProto , $arrIn , $msg , $pack); 
//var_dump($strOut);
//var_dump( md5(serialize($strOut)));
var_dump( md5($strOut) );


$desc = proto2desc( $filePath , $fileProto );
//var_dump($desc);
var_dump( md5(serialize($desc)) );

$arrOut = protobuf2array( $filePath , $fileProto , $strOut , $msg , $unpack);
//var_dump($arrOut);
var_dump( md5( serialize($arrOut) ) );

$s2 = array2protobufbydesc( $desc , $arrIn , $msg , $pack );
var_dump($s2);
var_dump( md5($s2) );
//var_dump( md5(serialize($s2)));

$arrOut2 = protobuf2arraybydesc( $desc , $strOut , $msg , $unpack);
var_dump( md5( serialize($arrOut2) ) );




