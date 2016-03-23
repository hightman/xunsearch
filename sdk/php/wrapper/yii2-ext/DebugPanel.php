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

use yii\debug\Panel;
use yii\helpers\ArrayHelper;
use yii\helpers\Url;
use yii\log\Logger;
use yii\helpers\Html;
use yii\web\View;
use Yii;

/**
 * Debugger panel that collects and displays xunsearch queries performed to server-side.
 *
 * @author hightman <hightman@twomice.net>
 * @since 1.4.9
 */
class DebugPanel extends Panel
{
	public $com = 'xunsearch';
	private $_timings;

	public function init()
	{
		$this->actions['xunsearch-query'] = [
			'class' => 'hightman\xunsearch\DebugAction',
			'panel' => $this,
			'com' => $this->com,
		];
	}

	/**
	 * @inheritdoc
	 */
	public function getName()
	{
		return 'Xunsearch';
	}

	/**
	 * @inheritdoc
	 */
	public function getSummary()
	{
		$timings = $this->calculateTimings();
		$queryCount = count($timings);
		$queryTime = 0;
		foreach ($timings as $timing) {
			$queryTime += $timing[3];
		}
		$queryTime = number_format($queryTime * 1000) . ' ms';
		$url = $this->getUrl();
		$output = <<<EOD
<div class="yii-debug-toolbar__block">
    <a href="$url" title="Executed $queryCount xunsearch queries which took $queryTime.">
        XS <span class="yii-debug-toolbar__label yii-debug-toolbar__label_info">$queryCount</span> <span class="yii-debug-toolbar__label">$queryTime</span>
    </a>
</div>
EOD;

		return $queryCount > 0 ? $output : '';
	}

	/**
	 * @inheritdoc
	 */
	public function getDetail()
	{
		$timings = $this->calculateTimings();
		ArrayHelper::multisort($timings, 3, SORT_DESC);
		$i = 0;
		foreach ($timings as $logId => $timing) {
			$duration = sprintf('%.1f ms', $timing[3] * 1000);
			$message = $timing[1];
			$traces = $timing[4];
			if (($pos = mb_strpos($message, "#")) !== false) {
				$url = mb_substr($message, 0, $pos);
				$body = mb_substr($message, $pos + 1);
			} else {
				$url = $message;
				$body = null;
			}
			$traceString = '';
			if (!empty($traces)) {
				$traceString .= Html::ul($traces, [
						'class' => 'trace',
						'item' => function ($trace) {
						return "<li>{$trace['file']}({$trace['line']})</li>";
					},
				]);
			}
			$runLink = '';
			if (($pos = strrpos($url, '.')) !== false) {
				$action = substr($url, $pos + 1);
				if (!strncmp($action, 'find', 4) || !strcmp($action, 'count')) {
					$runLink = Html::a('run query', '#', ['id' => "xun-link-$i"]);
					$ajaxUrl = Url::to(['xunsearch-query', 'logId' => $logId, 'tag' => $this->tag]);
					Yii::$app->view->registerJs(<<<JS
$('#xun-link-$i').on('click', function () {
    var result = $('#xun-result-$i');
    result.html('Sending request...');
    result.parent('tr').show();
    $.ajax({
        type: "POST",
        url: "$ajaxUrl",
        success: function (data) {
            $('#xun-time-$i').html(data.time);
            $('#xun-result-$i').html(data.result);
        },
        error: function (jqXHR, textStatus, errorThrown) {
            $('#xun-time-$i').html('');
            $('#xun-result-$i').html('<span style="color: #c00;">Error: ' + errorThrown + ' - ' + textStatus + '</span><br />' + jqXHR.responseText);
        },
        dataType: "json"
    });
    return false;
});
JS
						, View::POS_READY);
				}
			}
			$rows[] = <<<HTML
<tr>
    <td style="width: 10%;">$duration</td>
    <td style="width: 75%;"><div><b>$url</b><br/><p>$body</p>$traceString</div></td>
    <td style="width: 15%;">$runLink</td>
</tr>
<tr style="display: none;"><td id="xun-time-$i"></td><td colspan="3" id="xun-result-$i"></td></tr>
HTML;
			$i++;
		}
		$rows = implode("\n", $rows);

		return <<<HTML
<h1>Xunsearch Queries</h1>

<table class="table table-condensed table-bordered table-striped table-hover" style="table-layout: fixed;">
<thead>
<tr>
    <th style="width: 10%;">Time</th>
    <th style="width: 75%;">Query</th>
    <th style="width: 15%;">Run Query</th>
</tr>
</thead>
<tbody>
$rows
</tbody>
</table>
HTML;
	}

	public function calculateTimings()
	{
		if ($this->_timings !== null) {
			return $this->_timings;
		}
		$messages = $this->data['messages'];
		$timings = [];
		$stack = [];
		foreach ($messages as $i => $log) {
			list($token, $level, $category, $timestamp) = $log;
			$log[5] = $i;
			if ($level == Logger::LEVEL_PROFILE_BEGIN) {
				$stack[] = $log;
			} elseif ($level == Logger::LEVEL_PROFILE_END) {
				if (($last = array_pop($stack)) !== null && $last[0] === $token) {
					$timings[$last[5]] = [count($stack), $token, $last[3], $timestamp - $last[3], $last[4]];
				}
			}
		}

		$now = microtime(true);
		while (($last = array_pop($stack)) !== null) {
			$delta = $now - $last[3];
			$timings[$last[5]] = [count($stack), $last[0], $last[2], $delta, $last[4]];
		}
		ksort($timings);

		return $this->_timings = $timings;
	}

	/**
	 * @inheritdoc
	 */
	public function save()
	{
		$target = $this->module->logTarget;
		$messages = $target->filterMessages($target->messages, Logger::LEVEL_PROFILE, [ __NAMESPACE__ . '\*']);

		return ['messages' => $messages];
	}
}
