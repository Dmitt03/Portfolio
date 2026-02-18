#include "json_builder.h"
#include <variant>
#include <string>
#include <stdexcept>

using namespace std;

namespace json {

	Builder::Builder()
		: expected_({ ExpectedType::START_ARRAY, ExpectedType::START_DICT, ExpectedType::VALUE }) {
	}

	std::string Builder::GenerateTextError() const {
		std::string result("Expected:");

		if (expected_.contains(ExpectedType::KEY)) {
			result += "\nkey"s;
		}
		if (expected_.contains(ExpectedType::VALUE)) {
			result += "\nvalue"s;
		}
		if (expected_.contains(ExpectedType::START_DICT)) {
			result += "\nstart dict"s;
		}
		if (expected_.contains(ExpectedType::START_ARRAY)) {
			result += "\nstart array"s;
		}
		if (expected_.contains(ExpectedType::END_ARRAY)) {
			result += "\nend array"s;
		}
		if (expected_.contains(ExpectedType::END_DICT)) {
			result += "\nend dict"s;
		}

		return result;
	}

	void Builder::CheckExpected(ExpectedType type) const {
		if (type == ExpectedType::END_DICT) {
			if (!nodes_stack_.back()->IsMap()) {
				throw std::logic_error("dictionary to close not fount");
			}
		}
		if (type == ExpectedType::END_ARRAY) {
			if (!nodes_stack_.back()->IsArray()) {
				throw std::logic_error("array to close not fount");
			}
		}

		if (!expected_.contains(type)) {
			throw std::logic_error(GenerateTextError());
		}
	}


	Builder::KeyContext Builder::Key(std::string str) {
		CheckExpected(ExpectedType::KEY);
		expected_ = { ExpectedType::VALUE, ExpectedType::START_ARRAY, ExpectedType::START_DICT };

		current_key_ = std::move(str);
		return KeyContext(*this);
	}

	Node Builder::GetNodeFromValue(Node::Value&& val) const {
		if (holds_alternative<nullptr_t>(val)) {
			return Node(nullptr);
		}
		if (holds_alternative<Array>(val)) {
			return Node(get<Array>(std::move(val)));
		}
		if (holds_alternative<Dict>(val)) {
			return Node(get<Dict>(std::move(val)));
		}
		if (holds_alternative<bool>(val)) {
			return Node(get<bool>(val));
		}
		if (holds_alternative<int>(val)) {
			return Node(get<int>(val));
		}
		if (holds_alternative<double>(val)) {
			return Node(get<double>(val));
		}
		if (holds_alternative<string>(val)) {
			return Node(get<string>(std::move(val)));
		}
		throw std::runtime_error("returnint the node was not succesfully");
	}

	Builder& Builder::Value(Node::Value val) {
		CheckExpected(ExpectedType::VALUE);
		if (nodes_stack_.empty()) {	// Если это первый элемент		
			root_ = GetNodeFromValue(std::move(val));
			nodes_stack_.push_back(&root_);
			expected_ = {};
		}
		else {	// Если уже элементы были и value не будет root
			if (current_key_) {		// То есть находимся в словаре и нужно запихнуть значение для этого ключа, словарь лежит последним в стеке			
				Dict& dict = const_cast<Dict&>(nodes_stack_.back()->AsMap());
				dict[std::move(*current_key_)] = GetNodeFromValue(std::move(val));
				current_key_ = nullopt;
				expected_ = { ExpectedType::KEY, ExpectedType::END_DICT };
			}
			else {	// Значит находимся в массиве, этот массив лежит последним в стеке
				Array& arr = const_cast<Array&>(nodes_stack_.back()->AsArray());
				arr.push_back(GetNodeFromValue(std::move(val)));
				expected_ = { ExpectedType::START_ARRAY, ExpectedType::START_DICT, ExpectedType::VALUE, ExpectedType::END_ARRAY };
			}
		}
		return *this;
	}

	Builder::DictContext Builder::StartDict() {
		CheckExpected(ExpectedType::START_DICT);
		expected_ = { ExpectedType::KEY, ExpectedType::END_DICT };	// или EndDict, но его рассмотрим отдельно
		if (nodes_stack_.empty()) {
			root_ = Dict();
			nodes_stack_.push_back(&root_);
		}
		else {	// если добаляем словарь не в корень. Значит это или значение для ключа старшего словаря, или элемент массива
			if (current_key_) {	 // 1) если старше стоит словарь, а это будет значением для его ключа. А этот словарь лежит последним в стеке
				std::string key = std::move(*current_key_);
				current_key_.reset();
				Dict& dict = const_cast<Dict&>(nodes_stack_.back()->AsMap());	// Собственно, получили этот словарь
				dict[key] = Dict();
				nodes_stack_.push_back(&dict[key]);
			}
			else {	// 2) если это элемент массива. Массив лежит последним в стеке
				Array& arr = const_cast<Array&>(nodes_stack_.back()->AsArray());	// Уже в конец массива добавить словарь
				arr.emplace_back(Dict());
				nodes_stack_.push_back(&arr.back());
			}
		}
		return DictContext(*this);
	}

	Builder::ArrayContext Builder::StartArray() {
		CheckExpected(ExpectedType::START_ARRAY);
		expected_ = { ExpectedType::VALUE, ExpectedType::START_DICT, ExpectedType::START_ARRAY, ExpectedType::END_ARRAY };
		if (nodes_stack_.empty()) {
			root_ = Array();
			nodes_stack_.push_back(&root_);
		}
		else {	// Значит добавляем не в корень, а либо в 1) словарь, 2) другой массив
			if (current_key_) {		// 1) мы в словаре, значит в конце стека словарь
				std::string key = std::move(*current_key_);
				current_key_.reset();
				Dict& dict = const_cast<Dict&>(nodes_stack_.back()->AsMap());	// получили словарь
				dict[key] = Array();
				nodes_stack_.push_back(&dict[key]);
			}
			else {
				Array& arr = const_cast<Array&>(nodes_stack_.back()->AsArray());	// Уже в конец массива добавить другой массив
				arr.emplace_back(Array());
				nodes_stack_.push_back(&arr.back());
			}
		}
		return ArrayContext(*this);
	}

	Builder& Builder::EndDict() {
		CheckExpected(ExpectedType::END_DICT);
		nodes_stack_.pop_back();	// Удаляем последний элемент
		if (nodes_stack_.empty()) {
			expected_.clear();
		}
		else {
			Node* back_node = nodes_stack_.back();
			if (back_node->IsArray()) {
				expected_ = all_allowed;
			}
			else if (back_node->IsMap()) {
				expected_ = { ExpectedType::KEY, ExpectedType::END_DICT };
			}
			else {	// Если дошли до корня и это не массив/словарь, то делать ничего нельзя
				expected_.clear();
			}
		}
		current_key_ = nullopt;

		return *this;
	}

	Builder& Builder::EndArray() {
		CheckExpected(ExpectedType::END_ARRAY);
		nodes_stack_.pop_back();	// Удаляем последний элемент
		if (nodes_stack_.empty()) {
			expected_.clear();
		}
		else {
			Node* back_node = nodes_stack_.back();
			if (back_node->IsArray()) {
				expected_ = all_allowed;
			}
			else if (back_node->IsMap()) {
				expected_ = { ExpectedType::KEY, ExpectedType::END_DICT };
			}
			else {	// Если дошли до корня и это не массив/словарь, то делать ничего нельзя
				expected_.clear();
			}
		}
		return *this;
	}

	json::Node Builder::Build() {
		if (nodes_stack_.empty()) {
			if (root_.IsNull()) {
				throw logic_error("Root not found. Building terminated");
			}
			return root_;
		}
		else {
			// Проверяем, что все контейнеры закрыты
			if (nodes_stack_.size() > 1) {
				throw logic_error("Not all containers have been closed");
			}
			return root_;
		}
	}


} // namespace json