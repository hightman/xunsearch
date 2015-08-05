<?php
/**
 * xunsearch QueryBuilder class file
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2014 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
namespace hightman\xunsearch;

use Yii;
use yii\base\Object;
use yii\base\InvalidParamException;

/**
 * QueryBuilder builds query string based on the specification given as a [[ActiveQuery]] object.
 *
 * @author xjflyttp <xjflyttp@gmail.com>
 * @author hightman <hightman@twomice.net>
 * @since 1.4.9
 */
class QueryBuilder extends Object
{
	/**
	 * @var Database the database to be used.
	 */
	public $db;

	/**
	 * @var array map of query condition to builder methods.
	 * These methods are used by [[buildCondition]] to build SQL conditions from array syntax.
	 */
	protected $conditionBuilders = [
		'NOT' => 'buildNotCondition',
		'AND' => 'buildAndCondition',
		'OR' => 'buildAndCondition',
		'XOR' => 'buildAndCondition',
		'IN' => 'buildInCondition',
		'NOT IN' => 'buildInCondition',
		'BETWEEN' => 'buildBetweenCondition',
		'WEIGHT' => 'buildWeightCondition',
		'WILD' => 'buildAndCondition',
	];

	/**
	 * Constructor.
	 * @param Database $db the database
	 * @param array $config name-value pairs that will be used to initialize the object properties
	 */
	public function __construct(Database $db, $config = [])
	{
		$this->db = $db;
		parent::__construct($config);
	}

	/**
	 * Generates a query string from a [[ActiveQuery]] object.
	 * @param ActiveQuery $query
	 * @return \XSSearch ready XS search object
	 */
	public function build($query)
	{
		$others = [];
		if ($query->query === null) {
			$query->query = $this->buildWhere($query->where, $others);
		}

		$profile = $this->db->getName() . '.build#' . $query->query;
		Yii::beginProfile($profile, __METHOD__);
		$search = $this->db->getSearch();
		$search->setFuzzy($query->fuzzy)->setAutoSynonyms($query->synonyms);
		$search->setQuery($query->query);
		if (isset($others['range'])) {
			$this->buildRange($others['range']);
		}
		if (isset($others['weight'])) {
			$this->buildWeight($others['weight']);
		}
		if (is_callable($query->buildOther)) {
			call_user_func($query->buildOther, $search);
		}
		$this->buildLimit($query->limit, $query->offset);
		$this->buildOrderBy($query->orderBy);
		Yii::endProfile($profile, __METHOD__);
		return $search;
	}

	/**
	 * @param string|array $condition
	 * @param array $others used to save other query setting
	 * @return string the query string built from [[QueryTrait::$where]].
	 */
	protected function buildWhere($condition, &$others)
	{
		return $this->buildCondition($condition, $others);
	}

	/** 	
	 * @param array $ranges
	 */
	protected function buildRange($ranges)
	{
		foreach ($ranges as $range) {
			call_user_func_array(array($this->db->getSearch(), 'addRange'), $range);
		}
	}

	/**
	 * @param array $weights
	 */
	protected function buildWeight($weights)
	{
		foreach ($weights as $weight) {
			call_user_func_array(array($this->db->getSearch(), 'addWeight'), $weight);
		}
	}

	/**
	 * @param integer $limit
	 * @param integer $offset
	 */
	protected function buildLimit($limit, $offset)
	{
		$limit = intval($limit);
		$offset = max(0, intval($offset));
		if ($limit > 0) {
			$this->db->getSearch()->setLimit($limit, $offset);
		}
	}

	/**
	 * @param array $columns
	 */
	protected function buildOrderBy($columns)
	{
		$search = $this->db->getSearch();
		if (!empty($columns)) {
			if (count($columns) === 1) {
				foreach ($columns as $name => $direction) {
					$search->setSort($name, $direction === SORT_DESC ? false : true);
				}
			} else {
				$sorts = [];
				foreach ($columns as $name => $direction) {
					$sorts[$name] = $direction === SORT_DESC ? false : true;
				}
				$search->setMultiSort($sorts);
			}
		} else {
			$search->setSort(null);
		}
	}

	/**
	 * Parses the condition specification and generates the corresponding xunsearch query string.
	 * @param string|array $condition the condition specification. Please refer to [[QueryTrait::where()]]
	 * on how to specify a condition.
	 * @param array $others used to save other query setting
	 * @return string the generated query string
	 */
	protected function buildCondition($condition, &$others)
	{
		if (!is_array($condition)) {
			return strval($condition);
		} elseif (empty($condition)) {
			return '';
		}
		if (isset($condition[0])) {
			// operator format: operator, operand 1, operand 2, ...
			$operator = strtoupper($condition[0]);
			if (isset($this->conditionBuilders[$operator])) {
				$method = $this->conditionBuilders[$operator];
			} else {
				$method = 'buildSimpleCondition';
			}
			array_shift($condition);
			return $this->$method($operator, $condition, $others);
		} else {
			// hash format: 'column1' => 'value1', 'column2' => 'value2', ...
			return $this->buildHashCondition($condition, $others);
		}
	}

	/**
	 * Inverts a query string with `NOT` operator.
	 * @param string $operator the operator to use for connecting the given operands
	 * @param array $operands the query expressions to connect
	 * @param array $others used to save other query setting
	 * @return string the generated query string
	 * @throws InvalidParamException if wrong number of operands have been given.
	 */
	public function buildNotCondition($operator, $operands, &$others)
	{
		if (count($operands) !== 1) {
			throw new InvalidParamException("Operator '$operator' requires exactly one operand.");
		}
		$operand = reset($operands);
		if (is_array($operand)) {
			$operand = $this->buildCondition($operand, $others);
		} else {
			$operand = trim($operand);
		}
		if ($operand === '') {
			return '';
		} else {
			return $operator . ' ' . $this->smartBracket($operand);
		}
	}

	/**
	 * Connects two or more query expressions with the `AND` or `OR` or `WILD` operator.
	 * @param string $operator the operator to use for connecting the given operands
	 * @param array $operands the query expressions to connect.
	 * @param array $others used to save other query setting
	 * @return string the generated query string
	 */
	protected function buildAndCondition($operator, $operands, &$others)
	{
		$parts = [];
		foreach ($operands as $operand) {
			if (is_array($operand)) {
				$operand = $this->buildCondition($operand, $others);
			}
			$operand = trim($operand);
			if ($operand !== '') {
				$parts[] = $operand;
			}
		}
		if (count($parts) === 0) {
			return '';
		} elseif (count($parts) === 1) {
			return $parts[0];
		} else {
			for ($i = 0; $i < count($parts); $i++) {
				$parts[$i] = $this->smartBracket($parts[$i]);
			}
			$delimiter = $operator === 'WILD' ? ' ' : ' ' . $operator . ' ';
			return implode($delimiter, $parts);
		}
	}

	/**
	 * Creates a query string with the `IN` operator.
	 * @param string $operator the operator to use (e.g. `IN` or `NOT IN`)
	 * @param array $operands the first operand is the column name. If it is an array
	 * a composite IN condition will be generated.
	 * The second operand is an array of values that column value should be among.
	 * @return string the generated query string
	 */
	protected function buildInCondition($operator, $operands)
	{
		if (!isset($operands[0], $operands[1])) {
			throw new InvalidParamException("Operator '$operator' requires two operands.");
		}
		$parts = [];
		list($column, $values) = $operands;
		foreach ($values as $value) {
			$value = trim($value);
			if ($value !== '') {
				$parts[] = $column . ':' . $this->smartBracket($value);
			}
		}
		$query = implode(' OR ', $parts);
		if (substr($operator, 0, 3) === 'NOT') {
			$query = 'NOT ' . (count($parts) > 1 ? '(' . $query . ')' : $query);
		}
		return $query;
	}

	/**
	 * Creates an search value range.
	 * @param string $operator the operator to use (now only support `BETWEEN`)
	 * @param array $operands the first operand is the column name. The second and third operands
	 * describe the interval that column value should be in, null means unlimited.
	 * @param array $others used to save other query setting
	 * @return string the generated query string
	 * @throws InvalidParamException if wrong number of operands have been given.
	 */
	protected function buildBetweenCondition($operator, $operands, &$others)
	{
		if (!isset($operands[0], $operands[1], $operands[2])) {
			throw new InvalidParamException("Operator '$operator' requires three operands.");
		}
		$others['range'][] = $operands;
	}

	/**
	 * Creates a weigth query
	 * @param string $operator the operator to use (should be `WEIGHT`)
	 * @param array $operands the first operand is the column name. 
	 * The second operand is the term to adjust weight.
	 * The 3rd operand is optional float value, it means to weight scale, default to 1.
	 * @param array $others used to save other query setting
	 * @throws InvalidParamException
	 */
	protected function buildWeightCondition($operator, $operands, &$others)
	{
		if (!isset($operands[0], $operands[1])) {
			throw new InvalidParamException("Operator '$operator' requires two operands at least.");
		}
		$others['weight'][] = $operands;
	}

	protected function buildSimpleCondition($operator, $operands)
	{
		return $operator . ' ' . implode(' ', $operands);
	}

	/**
	 * Creates a condition based on column-value pairs.
	 * @param array $condition the condition specification.
	 * @return string the generated query string
	 */
	protected function buildHashCondition($condition)
	{
		$parts = [];
		foreach ($condition as $column => $value) {
			if (is_array($value)) {
				$pparts = [];
				foreach ($value as $v) {
					$v = trim($v);
					if ($v !== '') {
						$pparts[] = $column . ':' . $this->smartBracket($v);
					}
				}
				if (count($pparts) > 1) {
					$part = implode(' OR ', $pparts);
					if (count($condition) > 1) {
						$part = '(' . $part . ')';
					}
					$parts[] = $part;
				} elseif (count($pparts) === 1) {
					$parts[] = $pparts[0];
				}
			} elseif ($value !== null) {
				$value = trim($value);
				if ($value !== '') {
					$parts[] = $column . ':' . $this->smartBracket($value);
				}
			}
		}
		return implode(' ', $parts);
	}

	private function smartBracket($word)
	{
		if (strpos($word, ' ') === false || substr($word, 0, 4) === 'NOT ') {
			return $word;
		} else {
			return '(' . $word . ')';
		}
	}
}
