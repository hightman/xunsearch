<?php
require_once(dirname(__FILE__) . '/MarkdownParser.php');

class XMarkdown extends CMarkdown
{
	public $cssFile = false;

	protected function createMarkdownParser()
	{
		return new MarkdownParser;
	}

	public function _doImages_callback_format($match)
	{
		return '<div class="image"><p>' . (isset($match[2]) ? $match[2] : $match[1]) . '</p>' . $match[0] . '</div>';
	}

	public function transform($output)
	{
		// toc process
		$toc = '';
		$output = parent::transform($output);
		if (preg_match_all('/<h2(?: id=".+")?>(.+?)<\/h2>/', $output, $match, PREG_PATTERN_ORDER) > 1)
		{
			$toc = CHtml::openTag('div', array('class' => 'toc')) . CHtml::openTag('ol');
			for ($i = 0; $i < count($match[0]); $i++)
			{
				$text = ($i + 1) . '. ' . $match[1][$i] . CHtml::link('¶', null, array('name' => 'ch' . $i, 'class' => 'anchor'));
				$html = CHtml::tag('h2', array('id' => 'ch' . $i), $text);
				$toc .= Chtml::tag('li', array(), CHtml::link($match[1][$i], '#ch' . $i));
				$output = str_replace($match[0][$i], $html, $output);
			}
			$toc .= CHtml::closeTag('ol') . CHtml::closeTag('div');
		}
		// image process
		$output = preg_replace_callback('#<img\s+src=".+?"\s+alt="(.+?)"(?:\s+title="(.+?)")?\s+/>#s', array(&$this, '_doImages_callback_format'), $output);
		return $toc . $output;
	}
}
