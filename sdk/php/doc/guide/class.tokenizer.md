XSTokenizer 分词接口
===================

[XSTokenizer] 是搜索字段的分词接口定义，整个接口只要求实现一个方法 [XSTokenizer::getTokens]，
自定义分词器用于 INI 配置文件中的 `tokenizer` 选项。

关于自定义分词器的详细用法剖析请阅读后面的[专题](ini.tokenizer)。

<div class="revision">$Id$</div>
