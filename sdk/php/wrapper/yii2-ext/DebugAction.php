<?php
/**
 * xunsearch DebugPanel class file
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2014 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
namespace hightman\xunsearch;

use yii\base\Action;
use yii\base\NotSupportedException;
use yii\helpers\ArrayHelper;
use yii\helpers\Json;
use yii\web\HttpException;
use yii\web\Response;
use Yii;

/**
 * Debug Action is used by [[DebugPanel]] to perform xunsearch queries using ajax.
 *
 * @author hightman <hightman@twomice.net>
 * @since 1.4.9
 */
class DebugAction extends Action
{
	/**
	 * @var string the xunsearch component name
	 */
	public $com;

	/**
	 * @var DebugPanel
	 */
	public $panel;

	/**
	 * @var \yii\debug\controllers\DefaultController
	 */
	public $controller;

	public function run($logId, $tag)
	{
		$this->controller->loadData($tag);

		$timings = $this->panel->calculateTimings();
		ArrayHelper::multisort($timings, 3, SORT_DESC);
		if (!isset($timings[$logId])) {
			throw new HttpException(404, 'Log message not found.');
		}
		$message = $timings[$logId][1];
		if (($pos = mb_strpos($message, "#")) !== false) {
			$url = mb_substr($message, 0, $pos);
			$body = mb_substr($message, $pos + 1);
		} else {
			$url = $message;
			$body = null;
		}

		list($dbname, $action) = explode('.', $url);
		/* @var $db Database */
		$db = Yii::$app->get($this->com)->getDatabase($dbname);
		$time = microtime(true);

		switch ($action) {
			case 'findAll':
				$docs = $db->getSearch()->setLimit(3)->setQuery($body)->search();
				$result = '<strong>Estimated Matched: </strong>' . $db->getLastCount();
				foreach ($docs as $doc) {
					$result .= '<br/>' . $doc->rank() . '. (' . $doc->percent() . '%)';
					$result .= "<br/>" . Json::encode($doc->getFields(), 448) . "\n";
				}
				if ($db->getLastCount() > 3) {
					$result .= '<br/> ... other ' . ($db->getLastCount() - 3) . ' results ...';
				}
				break;
			case 'findOne':
				$docs = $db->getSearch()->setLimit(1)->setQuery($body)->search();
				if (count($docs) === 0) {
					$result = '<span class="label label-danger">no found</span>';
				} else {
					$result = "<br/>\n" . Json::encode($docs[0]->getFields(), 448);
				}
				break;
			case 'count':
				$count = $db->getSearch()->setQuery($body)->count();
				$result = '<strong>Estimated Matched: </strong>' . $count;
				break;
			default:
				throw new NotSupportedException("Action '$action' is not supported by xunsearch.");
		}
		$result = '<strong>DB Total: </strong>' . $db->getDbTotal() . '<br/>'
			. '<strong>Parsed Query: </strong>' . $db->getQuery() . '<br/>' . $result;

		Yii::$app->response->format = Response::FORMAT_JSON;
		return [
			'time' => sprintf('%.1f ms', (microtime(true) - $time) * 1000),
			'result' => $result,
		];
	}
}
