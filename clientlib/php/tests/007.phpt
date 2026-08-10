--TEST--
dataman validates forward and back because the key fields are not sequential this is only good for this test index and datafile
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
var_dump(dataman_forward('v2_index'));
var_dump($masterRecord[1]);
var_dump(dataman_back('v2_index'));
var_dump(dataman_back('v2_index'));
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
string(7) "0049291"
bool(true)
bool(true)
string(7) "0117225"
bool(true)

