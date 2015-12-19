<?php

use Goutte\Client;

define('REPO_URL', 'https://' . getenv('DIFFI_USER') . ':' . getenv('DIFFI_PASS') . '@github.com/weitzman/diffi.git');
define('REPO_PATH',  '/tmp/diffidata'); // drush_tempdir()

// Clone repo
// Loop recipes
//if new, get and add+commit
//else
//- curl
//- if changed, commit
//- push

function diffi_drush_command() {
  diffi_autoload();

  $items['diffi-loop'] = array(
    'bootstrap' => DRUSH_BOOTSTRAP_NONE,
  );
  return $items;
}

/**
 * .
 */
function drush_diffi_loop_init() {
  drush_shell_exec('rm -rf %s', REPO_PATH);
  // @todo Use shallow clone with git 1.9? https://blog.oio.de/2014/02/06/better-support-for-shallow-clones-in-git-1-9/
  $exec = 'git clone %s %s';
  if (!drush_shell_exec($exec, REPO_URL, REPO_PATH)) {
    return drush_set_error('Unable to git clone.');
  }
  else {
    drush_shell_cd_and_exec(REPO_PATH, 'git config user.email "diffibot@diffi.com"');
    drush_shell_cd_and_exec(REPO_PATH, 'git config user.name "Diffi Bot"');
    drush_shell_cd_and_exec(REPO_PATH, 'git config push.default simple');
  }
}

/**
 * Command callback.
 */
function drush_diffi_loop() {
  $path = dirname(__DIR__) . '/recipes';
  $it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($path));
  $it->rewind();
  while($it->valid()) {
    if (!$it->isDir()) {
      drush_log('Fetch ' . $it->key());
      $recipe = json_decode(file_get_contents($it->getPathname()));

      $client = new Client();
      $crawler = $client->request('GET', $recipe->url);
      $status = $client->getResponse()->getStatus();
      if ($status != 200) {
        drush_log($recipe->url . ' returned ' . $status, 'warning');
        continue;
      }
      $html = $crawler->filter($recipe->selector)->html();
      $return = diffi_commit($html, $it);
    }
    $it->next();
  }
  diffi_push(REPO_PATH);
}

function diffi_commit($html, $it) {
  $target = diffi_target($it);
  file_force_contents($target, $html);

  $exec = 'git add %s && git commit -am "%s"';
  $msg = 'Update to ' . $target . '.';
  $return = drush_shell_cd_and_exec(dirname($target), $exec, $target, $target);
}

function diffi_push ($repo_path) {
  $exec = 'git push';
  if (!$return = drush_shell_cd_and_exec($repo_path, $exec)) {
    return drush_set_error('Unable to git push');
  }
}

function diffi_autoload() {
  $local_vendor_path = __DIR__ . '/../vendor/autoload.php';
  $global_vendor_path = __DIR__ . '/../../../../vendor/autoload.php';

  // Check for a local composer install or a global composer install. Vendor
  // dirs are in different spots).
  if ((!@include $local_vendor_path) && (!@include $global_vendor_path)) {
    drush_log("Unable to load autoload.php. Diffi requires Composer in order to install its depedencies and autoload classes.", 'warning');
    return array();
  }
}

function diffi_target($it) {
  $recipe_path = $it->getPathname();
  $target_file = basename($recipe_path);
  $target_directory = REPO_PATH . '/' . (basename(dirname($recipe_path)));
  return $target_directory . '/' . str_replace('.json', '.html', $target_file);
}

// Creates dirs if needed then file_put_contents().
// http://php.net/manual/en/function.file-put-contents.php#84180
function file_force_contents($dir, $contents){
  $parts = explode('/', $dir);
  $file = array_pop($parts);
  $dir = '';
  foreach($parts as $part)
    if(!is_dir($dir .= "/$part")) mkdir($dir);
  return file_put_contents("$dir/$file", $contents);
}