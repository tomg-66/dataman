--TEST--
Dataman extension API is registered
--EXTENSIONS--
dataman
--FILE--
<?php
var_dump(extension_loaded('dataman'));
var_dump(RDONLY, UPDATE, BEFORE, AFTER);
var_dump(function_exists('dataman_get'));
var_dump(class_exists('masterRecord'));
var_dump(class_exists('workRecord'));
?>
--EXPECT--
bool(true)
int(0)
int(1)
int(0)
int(1)
bool(true)
bool(true)
bool(true)
