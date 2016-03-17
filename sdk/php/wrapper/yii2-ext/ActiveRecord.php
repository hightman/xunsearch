<?php
/**
 * xunsearch ActiveRecord class file
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2014 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
namespace hightman\xunsearch;

use Yii;
use yii\db\BaseActiveRecord;
use yii\helpers\StringHelper;

/**
 * xunsearch AR
 * 
 * @property-write $internalDoc internal XSDocument object of search result
 * 
 * The following magic methods only for AR object returned by ActiveQuery
 * @method int docid() docid(void)
 * @method int rank() rank(void)
 * @method int percent() percent(void)
 * @method float weight() weight(void)
 * @method int ccount() ccount(void)
 * @method array matched() matched(void)
 * 
 * @author xjflyttp <xjflyttp@gmail.com>
 * @author hightman <hightman@twomice.net>
 * @since 1.4.9
 */
class ActiveRecord extends BaseActiveRecord
{
	/**
	 * @var \XSDocument internal document
	 */
	private $_internalDoc;

	/**
	 * @return string XS project name
	 */
	public static function projectName()
	{
		return strtolower(StringHelper::basename(get_called_class()));
	}

	/**
	 * Returns the database used by this AR class.
	 * By default, the "xunsearch" application component with [[projectName()]] is used to open the database.
	 * You may override this method if you want to use a different database.
	 * @return Database
	 */
	public static function getDb()
	{
		return Yii::$app->get('xunsearch')->getDatabase(static::projectName());
	}

	/**
	 * Returns the primary key name(s) for this AR class.
	 * You need not overridden this method.
	 *
	 * @return string[] the primary keys of this record.
	 */
	public static function primaryKey()
	{
		$db = static::getDb();
		return [$db->getFieldId()->name];
	}

	/**
	 * @inheritdoc
	 * @return ActiveQuery
	 */
	public static function find()
	{
		return Yii::createObject(ActiveQuery::className(), [get_called_class()]);
	}

	/**
	 * Creates an [[ActiveQuery]] instance with a given query statement.
	 * @param string $q the search query statement
	 * @return ActiveQuery the newly created [[ActiveQuery]] instance
	 */
	public static function findByQuery($q)
	{
		$query = static::find();
		$query->query = $q;
		return $query;
	}

	/**
	 * Updates the whole database using the provided attribute values and conditions.
	 * For example, to change the status to be 1 for all customers whose status is 2:
	 *
	 * ```php
	 * Customer::updateAll(['status' => 1], ['status' => '2']);
	 * ```
	 *
	 * @param array $attributes attribute values (name-value pairs) to be saved into the table
	 * @param string|array $condition the conditions that will be converted to query string
	 * @return integer the number of rows updated
	 */
	public static function updateAll($attributes, $condition = '')
	{
		$count = 0;
		$records = static::find()->where($condition)->all();
		foreach ($records as $record) {
			$record->setAttributes($attributes);
			if ($record->update() === true) {
				$count++;
			}
		}
		return $count;
	}

	/**
	 * Deletes rows in the table using the provided conditions.
	 * WARNING: If you do not specify any condition, this method will delete ALL rows in the database.
	 *
	 * For example, to delete all customers whose status is 3:
	 *
	 * ```php
	 * Customer::deleteAll(['status' => 3]);
	 * ```
	 *
	 * @param array $condition the conditions that will be converted to query string.
	 * @return integer the number of records deleted
	 */
	public static function deleteAll($condition = null)
	{
		$pks = self::fetchPks($condition);
		if (empty($pks)) {
			return 0;
		}
		$db = static::getDb();
		$profile = $db->getName() . '.deleteAll#' . implode(',', $pks);

		Yii::beginProfile($profile, __METHOD__);
		$db->getIndex()->del($pks);
		Yii::endProfile($profile, __METHOD__);

		return count($pks);
	}

	/**
	 * Populates an active record object using a xunsearch result document
	 *
	 * @param ActiveRecord $record the record to be populated.
	 * @param \XSDocument $doc
	 */
	public static function populateRecord($record, $doc)
	{
		parent::populateRecord($record, $doc->getFields());
		$record->setInternalDoc($doc);
	}

	/**
	 * @return \XSDocument internal document
	 */
	protected function getInternalDoc()
	{
		if ($this->_internalDoc === null) {
			$this->_internalDoc = static::getDb()->createDoc();
		}
		return $this->_internalDoc;
	}

	/**
	 * @param \XSDocument $doc
	 */
	public function setInternalDoc(\XSDocument $doc)
	{
		$this->_internalDoc = $doc;
	}

	/**
	 * Magic calls for populated AR object.
	 * 
	 * @param string $name the method name
	 * @param array $params method parameters
	 * @throws UnknownMethodException when calling unknown method
	 * @return mixed the method return value
	 */
	public function __call($name, $params)
	{
		if ($this->_internalDoc instanceof \XSDocument) {
			try {
				return call_user_func_array(array($this->_internalDoc, $name), $params);
			} catch (\Exception $e) {
				
			}
		}
		return parent::__call($name, $params);
	}

	/**
	 * Returns the list of all attribute names of the record.
	 * You need not overridden this method.
	 * 
	 * @return array list of attribute names.
	 */
	public function attributes()
	{
		$db = static::getDb();
		return array_keys($db->getAllFields());
	}

	/**
	 * The primary key is required, all others are safe.
	 * @return array validation rules for attributes.
	 */
	public function rules()
	{
		return [
			[static::primaryKey(), 'required'],
			[$this->attributes(), 'safe'],
		];
	}

	/**
	 * Add column index term
	 * @param string $column
	 * @param string $term
	 * @param int $wdf
	 * @see http://www.xunsearch.com/doc/php/api/XSDocument#addIndex-detail
	 * @return static the query object itself.
	 */
	public function addTerm($column, $term, $wdf = 1)
	{
		$this->getInternalDoc()->addTerm($column, $term, $wdf);
	}

	/**
	 * Add column index text
	 * @param string $column
	 * @param string $text
	 * @see http://www.xunsearch.com/doc/php/api/XSDocument#addTerm-detail
	 * @return static the query object itself.
	 */
	public function addIndex($column, $text)
	{
		$this->getInternalDoc()->addIndex($column, $text);
	}

	/**
	 * Inserts a row into the associated database table using the attribute values of this record.
	 *
	 * This method performs the following steps in order:
	 *
	 * 1. call [[beforeValidate()]] when `$runValidation` is true. If validation
	 *    fails, it will skip the rest of the steps;
	 * 2. call [[afterValidate()]] when `$runValidation` is true.
	 * 3. call [[beforeSave()]]. If the method returns false, it will skip the
	 *    rest of the steps;
	 * 4. insert the record into database. If this fails, it will skip the rest of the steps;
	 * 5. call [[afterSave()]];
	 *
	 * In the above step 1, 2, 3 and 5, events [[EVENT_BEFORE_VALIDATE]],
	 * [[EVENT_BEFORE_INSERT]], [[EVENT_AFTER_INSERT]] and [[EVENT_AFTER_VALIDATE]]
	 * will be raised by the corresponding methods.
	 *
	 * Note: internal implemention is full update for the whole document.
	 *
	 * For example, to insert a demo record:
	 *
	 * ```php
	 * $demo = new Demo;
	 * $demo->pid = 1;
	 * $demo->subject = 'hello';
	 * $demo->message = 'the world';
	 * $demo->insert();
	 * ```
	 *
	 * @param boolean $runValidation whether to perform validation before saving the record.
	 * If the validation fails, the record will not be inserted into the database.
	 * @param array $attributes list of attributes that need to be saved. Defaults to null,
	 * meaning all attributes that are loaded from DB will be saved.
	 * @return boolean whether the attributes are valid and the record is inserted successfully.
	 * @throws \Exception in case insert failed.
	 */
	public function insert($runValidation = true, $attributes = null)
	{
		if ($runValidation && !$this->validate($attributes)) {
			return false;
		}
		if (!$this->beforeSave(true)) {
			return false;
		}
		$db = static::getDb();
		$profile = $db->getName() . '.insert#' . $this->getPrimaryKey();
		$values = $this->getDirtyAttributes($attributes);

		Yii::beginProfile($profile, __METHOD__);
		$this->getInternalDoc()->setFields($values);
		$db->getIndex()->update($this->getInternalDoc());
		Yii::endProfile($profile, __METHOD__);

		$changedAttributes = array_fill_keys(array_keys($values), null);
		$this->setOldAttributes($values);
		$this->afterSave(true, $changedAttributes);
		return true;
	}

	/**
	 * Saves the changes to this active record into the associated database table.
	 *
	 * This method performs the following steps in order:
	 *
	 * 1. call [[beforeValidate()]] when `$runValidation` is true. If validation
	 *    fails, it will skip the rest of the steps;
	 * 2. call [[afterValidate()]] when `$runValidation` is true.
	 * 3. call [[beforeSave()]]. If the method returns false, it will skip the
	 *    rest of the steps;
	 * 4. save the record into database. If this fails, it will skip the rest of the steps;
	 * 5. call [[afterSave()]];
	 *
	 * In the above step 1, 2, 3 and 5, events [[EVENT_BEFORE_VALIDATE]],
	 * [[EVENT_BEFORE_UPDATE]], [[EVENT_AFTER_UPDATE]] and [[EVENT_AFTER_VALIDATE]]
	 * will be raised by the corresponding methods.
	 *
	 * Note: internal implemention is full update for the whole document.
	 *
	 * For example, to update a demo record:
	 *
	 * ```php
	 * $demo = Demo::findOne($id);
	 * $demo->subject = 'hello';
	 * $demo->message = 'the world';
	 * $demo->update();
	 * ```
	 *
	 * @param boolean $runValidation whether to perform validation before saving the record.
	 * If the validation fails, the record will not be inserted into the database.
	 * @param array $attributes list of attribute names that need to be saved. Defaults to null,
	 * meaning all attributes that are loaded from DB will be saved.
	 * @return boolean whether the attributes are valid and the record is updated successfully.
	 * @throws \Exception in case update failed.
	 */
	public function update($runValidation = true, $attributes = null)
	{
		if ($runValidation && !$this->validate($attributes)) {
			return false;
		}
		if (!$this->beforeSave(true)) {
			return false;
		}
		$values = $this->getDirtyAttributes($attributes);
		if (empty($values)) {
			$this->afterSave(false, $values);
			return 0;
		}
		$db = static::getDb();
		$profile = $db->getName() . '.update#' . $this->getPrimaryKey();

		Yii::beginProfile($profile, __METHOD__);
		$this->getInternalDoc()->setFields($this->getAttributes($attributes));
		$db->getIndex()->update($this->getInternalDoc());
		Yii::endProfile($profile, __METHOD__);

		$changedAttributes = [];
		foreach ($values as $name => $value) {
			$changedAttributes[$name] = $this->getOldAttribute($name);
			$this->setOldAttribute($name, $value);
		}
		$this->afterSave(false, $changedAttributes);
		return true;
	}

	/**
	 * Deletes the table row corresponding to this active record.
	 *
	 * This method performs the following steps in order:
	 *
	 * 1. call [[beforeDelete()]]. If the method returns false, it will skip the
	 *    rest of the steps;
	 * 2. delete the record from the database;
	 * 3. call [[afterDelete()]].
	 *
	 * In the above step 1 and 3, events named [[EVENT_BEFORE_DELETE]] and [[EVENT_AFTER_DELETE]]
	 * will be raised by the corresponding methods.
	 *
	 * @return boolean whether the record is removed successfully.
	 * @throws \Exception in case delete failed.
	 */
	public function delete()
	{
		if (($result = $this->beforeDelete()) !== false) {
			$pk = $this->getPrimaryKey();
			$db = static::getDb();
			$profile = $db->getName() . '.delete#' . $pk;

			Yii::beginProfile($profile, __METHOD__);
			$db->getIndex()->del($pk);
			Yii::endProfile($profile, __METHOD__);

			$this->setOldAttributes(null);
			$this->afterDelete();
		}
		return $result;
	}

	/**
	 * @param mixed $condition
	 * @return array
	 */
	private static function fetchPks($condition)
	{
		$primaryKey = static::primaryKey();
		$records = static::find()->where($condition)->asArray()->all();
		$pks = [];
		foreach ($records as $record) {
			$pk = $record[$primaryKey[0]];
			$pks[] = $pk;
		}
		return $pks;
	}
}
