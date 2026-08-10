--TEST--
dataman validates record file descriptions without validations
--EXTENSIONS--
dataman
--FILE--
<?php

try {
  var_dump($masterRecord[1]);
} catch (Throwable $e) {
  echo get_class($e), ": ", $e->getMessage(), "\n";
}
?>
--EXPECT--
Exception: Error: No current master record
