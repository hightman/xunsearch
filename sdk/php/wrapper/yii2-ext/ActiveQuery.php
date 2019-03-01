<?php
/**
 * xunsearch ActiveQuery class file
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2014 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
namespace hightman\xunsearch;

use Yii;
use yii\base\Component;
use yii\db\ActiveQueryInterface;
use yii\db\ActiveQueryTrait;
use yii\db\ActiveRelationTrait;
use yii\db\QueryTrait;

/**
 * ActiveQuery represents a XS query associated with an ActiveRecord class.
 *
 * An ActiveQuery can be a normal query or be used in a relational context.
 *
 * ActiveQuery instances are usually created by [[ActiveRecord::find()]] and [[ActiveRecord::findByQuery()]].
 * Relational queries are created by [[ActiveRecord::hasOne()]] and [[ActiveRecord::hasMany()]].
 *
 * Normal Query
 * ------------
 *
 * ActiveQuery mainly provides the following methods to retrieve the query results:
 *
 * - [[one()]]: returns a single record populated with the first row of data.
 * - [[all()]]: returns all records based on the query results.
 * - [[count()]]: returns the number of records.
 * - [[exists()]]: returns a value indicating whether the query result has data or not.
 *
 * Because ActiveQuery use [[QueryTrait]], one can use query methods, such as [[where()]],
 * [[orderBy()]] to customize the query options.
 *
 * ActiveQuery also provides the following additional query options:
 *
 * - [[with()]]: list of relations that this query should be performed with.
 * - [[indexBy()]]: the name of the column by which the query result should be indexed.
 * - [[asArray()]]: whether to return each record as an array.
 *
 * These options can be configured using methods of the same name. For example:
 *
 * ```php
 * $docs = Demo::find()->with('orders')->asArray()->all();
 * ```
 *
 * Relational query
 * ----------------
 *
 * In relational context ActiveQuery represents a relation between two Active Record classes.
 *
 * Relational ActiveQuery instances are usually created by calling [[ActiveRecord::hasOne()]] and
 * [[ActiveRecord::hasMany()]]. An Active Record class declares a relation by defining
 * a getter method which calls one of the above methods and returns the created ActiveQuery object.
 *
 * A relation is specified by [[link]] which represents the association between columns
 * of different tables; and the multiplicity of the relation is indicated by [[multiple]].
 *
 * If a relation involves a junction table, it may be specified by [[via()]] or [[viaTable()]] method.
 * These methods may only be called in a relational context. Same is true for [[inverseOf()]], which
 * marks a relation as inverse of another relation and [[onCondition()]] which adds a condition that
 * is to be added to relational query join condition.
 * 
 * @property-read Database $db default database to use
 * @property-read \XSSearch $search
 *
 * @author xjflyttp <xjflyttp@gmail.com>
 * @author hightman <hightman@twomice.net>
 * @since 1.4.9
 */
class ActiveQuery extends Component implements ActiveQueryInterface
{
	use ActiveQueryTrait;
	use ActiveRelationTrait;
	use QueryTrait;
	/**
	 * @event Event an event that is triggered when the query is initialized via [[init()]].
	 */
	const EVENT_INIT = 'init';
	const EVENT_BEFORE_SEARCH = 'beforeSearch';

	/**
	 * @var string the query string, this is set by [[ActiveRecord::findByQuery()]].
	 */
	public $query;

	/**
	 * @var boolean fuzzy search
	 * @see http://www.xunsearch.com/doc/php/api/XSSearch#setFuzzy-detail
	 */
	public $fuzzy = false;

	/**
	 * @var boolean expand synonyms automatically
	 * @see http://www.xunsearch.com/doc/php/api/XSSearch#setAutoSynonyms-detail
	 */
	public $synonyms = false;

	/**
	 * @var callable
	 */
	public $buildOther;

	/**
	 * @var \XSSearch
	 */
	private $_search;

	/**
	 * Constructor.
	 * @param array $modelClass the model class associated with this query
	 * @param array $config configurations to be applied to the newly created query object
	 */
	public function __construct($modelClass, $config = [])
	{
		$this->modelClass = $modelClass;
		parent::__construct($config);
	}

	/**
	 * Initializes the object.
	 * This method is called at the end of the constructor. The default implementation will trigger
	 * an [[EVENT_INIT]] event. If you override this method, make sure you call the parent implementation at the end
	 * to ensure triggering of the event.
	 */
	public function init()
	{
		parent::init();
		$this->trigger(self::EVENT_INIT);
	}

	/**
	 * @return Database default xunsearch database
	 */
	public function getDb()
	{
		$modelClass = $this->modelClass;
		return $modelClass::getDb();
	}

	/**
	 * return \XSSearch current XS search object
	 */
	public function getSearch()
	{
		return $this->_search;
	}

	/**
	 * Enable fuzzy search
	 * @param boolean $fuzzy
	 * @return static the query object itself.
	 */
	public function fuzzy($fuzzy = true)
	{
		$this->fuzzy = $fuzzy === true;
		return $this;
	}

	/**
	 * Enable synonyms search
	 * @param boolean $synonyms
	 * @return static the query object itself.
	 */
	public function synonyms($synonyms = true)
	{
		$this->synonyms = $synonyms === true;
		return $this;
	}

	/**
	 * Build other search options, such as weight, collapse etc.
	 * 
	 * ```php
	 * $finder = Demo::find();
	 * $finder->where('hello')->buildOther(function(\XSSearch $search) {
	 *   $search->addWeight('subject', 'hi', 1);
	 * })->asArray()->all();
	 * ```
	 * @param callable $callable a PHP callable that contains setting before searching
	 * @return static the query object itself.
	 */
	public function buildOther(callable $callable)
	{
		$this->buildOther = $callable;
		return $this;
	}

	/**
	 * @param weight
	 */
	protected function beforeSearch()
	{
		$this->trigger(self::EVENT_BEFORE_SEARCH);
	}

	/**
	 * Executes query and returns all results as an array.
	 * @param Database $db the database used to execute the query.
	 * If null, the DB returned by [[modelClass]] will be used.
	 * @return array|ActiveRecord[] the search results. If the results in nothing, an empty array will be returned.
	 */
	public function all($db = null)
	{
		$query = $this->query;
		$search = $this->buildSearch($db);
		$this->beforeSearch();
		$profile = $db->getName() . '.findAll#' . $this->query;
		Yii::beginProfile($profile, __METHOD__);
		$docs = $search->search($query);
		Yii::endProfile($profile, __METHOD__);
		return $this->populate($docs);
	}

	/**
	 * Executes query and returns a single row of result.
	 * @param Database $db the database used to execute the query.
	 * If null, the DB returned by [[modelClass]] will be used.
	 * @return ActiveRecord|array|null a single row of query result. Depending on the setting of [[asArray]],
	 * the query result may be either an array or an ActiveRecord object. Null will be returned
	 * if the query results in nothing.
	 */
	public function one($db = null)
	{
		$query = $this->query;
		$search = $this->buildSearch($db)->setLimit(1);
		$this->beforeSearch();
		$profile = $db->getName() . '.findOne#' . $this->query;
		Yii::beginProfile($profile, __METHOD__);
		$docs = $search->search($query);
		Yii::endProfile($profile, __METHOD__);
		$models = $this->populate($docs);
		if (count($models) === 0) {
			return null;
		} else {
			return $models[0];
		}
	}

	/**
	 * Returns the number of records.
	 * @param string $q the COUNT query. Defaults to '*'.
	 * @param Database $db the database used to execute the query.
	 * If null, the DB returned by [[modelClass]] will be used.
	 * @return integer number of records
	 */
	public function count($q = '*', $db = null)
	{
		if ($q !== '*') {
			$this->query = $q;
		}
		$query = $this->query;
		$search = $this->buildSearch($db);
		$profile = $db->getName() . '.count#' . $this->query;
		Yii::beginProfile($profile, __METHOD__);
		$count = $search->count($query);
		Yii::endProfile($profile, __METHOD__);
		return $count;
	}

	/**
	 * Returns a value indicating whether the query result contains any row of data.
	 * @param Database $db the database connection used to execute the query.
	 * @return boolean whether the query result contains any row of data.
	 */
	public function exists($db = null)
	{
		return $this->one($db) !== null;
	}

	/**
	 * @inheritdoc
	 */
	public function where($condition)
	{
		$this->query = null;
		$this->where = $condition;
		return $this;
	}

	/**
	 * Converts the found docs into the format as specified by this query.
	 * @param \XSDocument[] $docs the raw query result from database
	 * @return array|ActiveRecord[] the converted query result
	 */
	private function populate($docs)
	{
		if (empty($docs)) {
			return [];
		}
		$models = [];
		$class = $this->modelClass;
		foreach ($docs as $doc) {
			if ($this->asArray) {
				$model = $doc->getFields();
				/*
				  $model['__docid'] = $doc->docid();
				  $model['__percent'] = $doc->percent();
				  $model['__weight'] = $doc->weight();
				  $model['__ccount'] = $doc->ccount();
				  $model['__matched'] = $doc->matched();
				 */
			} else {
				$model = $class::instantiate($doc);
				$class::populateRecord($model, $doc);
			}
			if ($this->indexBy === null) {
				$models[] = $model;
			} else {
				if (is_string($this->indexBy)) {
					$key = $doc[$this->indexBy];
				} else {
					$key = call_user_func($this->indexBy, $model);
				}
				$models[$key] = $model;
			}
		}
		if (!empty($this->with)) {
			$this->findWith($this->with, $models);
		}
		if (!$this->asArray) {
			foreach ($models as $model) {
				$model->afterFind();
			}
		}
		return $models;
	}

	/**
	 * Prepare for searching and build it
	 * @param Database $db the database used to perform search.
	 * @return \XSSearch ready XS search object
	 */
	private function buildSearch(&$db)
	{
		if ($db === null) {
			$db = $this->getDb();
		}
		return $this->_search = $db->getQueryBuilder()->build($this);
	}
}
