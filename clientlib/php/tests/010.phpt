--TEST--
dataman validates delete, insert, include
--EXTENSIONS--
dataman
--FILE--
<?php
var_dump(dataman_connect([
	'-r', '/home/tomg/test',
	'-h', 'localhost',
]));
var_dump(dataman_iopen('one_rec_idx', UPDATE));
var_dump(dataman_get_first('one_rec_idx'));
var_dump($masterRecord[1]);
var_dump(dataman_delete('one_rec_idx'));
var_dump($masterRecord[1]);
var_dump(dataman_insert(2, AFTER, 'one_rec_idx'));
$masterRecord[2] = "this is a test";
var_dump($masterRecord[2]);
var_dump(dataman_insert(1, AFTER, 'one_rec_idx'));
$masterRecord[1] = "0131572";
$masterRecord[3] = "88888888";
var_dump($masterRecord[1]);
var_dump($masterRecord[3]);
var_dump($masterRecord[2]);
var_dump(dataman_include('one_rec_idx', 'one_rec_idx', $masterRecord[1]));
var_dump(dataman_back('one_rec_idx'));
var_dump(dataman_delete("one_rec_idx"));
var_dump(dataman_iclose('one_rec_idx'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
string(7) "0024817"
php_user_script: delete error: you can't delete the only rec in the file
bool(false)
string(7) "0024817"
bool(true)
string(3) "thi"
bool(true)
string(7) "0131572"
string(3) "888"
string(9) "         "
bool(true)
bool(true)
bool(true)
bool(true)
