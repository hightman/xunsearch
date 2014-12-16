<?php

class SiteController extends CController
{

	public function actionIndex()
	{
		$files = array();
		$d = dir('../doc');
		while (($e = $d->read()) !== false) {
			$ord = ord($e[0]);
			if ($ord < 65 || $ord > 90) {
				continue;
			}
			$files[] = $e;
		}
		$d->close();
		$this->render('index', array('files' => $files));
	}

	public function actionDoc($name)
	{
		$this->render('index', array('name' => $name));
	}

	public function actionApi($name = null)
	{
		readfile('../doc/html/api/' . $name . '.html');
		exit(0);
	}

	public function actionGuide($name = null)
	{
		if ($name === null || empty($name))
			$name = 'start.overview';
		$this->render('guide', array('name' => $name, 'toc' => $this->parseToc($name)));
	}

	// gen menuItems, prevItem, nextItem, ...
	private function parseToc($name)
	{
		$ret = array('items' => array(), 'prev' => null, 'next' => null);
		$lines = file('../doc/guide/toc.txt');

		// dot flag: *, -
		$matched = false;
		foreach ($lines as $line) {
			$line = trim($line);
			if ($line[0] === '*') {
				$ret['items'][] = array('label' => substr($line, 2));
			} elseif ($line[0] === '-') {
				$end = count($ret['items']) - 1;
				if (!isset($ret['items'][$end])) {
					continue;
				} elseif (!isset($ret['items'][$end]['items'])) {
					$ret['items'][$end]['items'] = array();
				}
				list ($label, $file) = explode('](', substr($line, 3, -1), 2);
				$ret['items'][$end]['items'][] = array('label' => $label, 'url' => array('guide/' . $file));

				if ($matched === true) {
					$cnt = count($ret['items'][$end]['items']);
					$ret['next'] = $ret['items'][$end]['items'][$cnt - 1];
					$matched = false;
				} elseif ($name === $file) {
					$cnt = count($ret['items'][$end]['items']);
					if ($cnt > 1) {
						$ret['prev'] = $ret['items'][$end]['items'][$cnt - 2];
					} elseif ($end > 0 && isset($ret['items'][$end - 1]['items'])) {
						$cnt = count($ret['items'][$end - 1]['items']);
						$ret['prev'] = $ret['items'][$end - 1]['items'][$cnt - 1];
					}
					$matched = true;
				}
			}
		}
		return $ret;
	}
}
