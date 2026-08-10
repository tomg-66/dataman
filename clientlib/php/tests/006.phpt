--TEST--
dataman validates get_next and get_prior
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
var_dump(dataman_get_next('v2_index'));
var_dump(dataman_get_next('v2_index'));
var_dump(dataman_get_next('v2_index'));
var_dump($masterRecord[1]);
var_dump(dataman_get_prior('v2_index'));
var_dump(dataman_get_prior('v2_index'));
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
bool(true)
string(7) "0003250"
bool(true)
bool(true)
string(7) "0003248"
bool(true)

