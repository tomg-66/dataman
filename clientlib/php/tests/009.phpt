--TEST--
dataman validates get_current
--EXTENSIONS--
dataman
--FILE--
<?php

var_dump(dataman_connect([
	'-r', '/home/tomg/test',
	'-h', 'localhost',
]));
var_dump(dataman_iopen('v2_index', RDONLY));
var_dump(dataman_get('v2_index', '0003247'));
var_dump($masterRecord[1]);
var_dump(dataman_forward('v2_index'));
var_dump(dataman_forward('v2_index'));
var_dump($masterRecord[1]);
var_dump(dataman_forward('v2_index'));
var_dump($masterRecord[1]);
var_dump(dataman_get_current('v2_index'));
var_dump($masterRecord[1]);
var_dump(dataman_iclose('v2_index'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
string(7) "0003247"
bool(true)
bool(true)
string(7) "0100448"
bool(true)
string(7) "0049291"
bool(true)
string(7) "0003247"
bool(true)

