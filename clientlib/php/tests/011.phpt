--TEST--
dataman validates delete and auto-remove it should be run immediately after 010.phpt, not independently
--EXTENSIONS--
dataman
--FILE--
<?php
var_dump(dataman_connect([
	'-r', '/home/tomg/test',
	'-h', 'localhost',
]));
var_dump(dataman_iopen('one_rec_idx', UPDATE));
var_dump(dataman_get('one_rec_idx', '0131572'));
var_dump($masterRecord[1]);
var_dump(dataman_delete('one_rec_idx'));
var_dump(dataman_get_current('one_rec_idx'));
var_dump(dataman_iclose('one_rec_idx'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
string(7) "0131572"
bool(true)
bool(false)
bool(true)
