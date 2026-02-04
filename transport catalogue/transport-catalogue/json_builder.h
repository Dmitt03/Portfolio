#include "json.h"
#include <optional>
#include <unordered_set>

namespace json {

	enum class ExpectedType
	{
		KEY,
		VALUE,
		START_DICT,
		START_ARRAY,
		END_DICT,
		END_ARRAY
	};

	struct EnumClassHash {
		size_t operator()(ExpectedType t) const {
			return static_cast<size_t>(t);
		}
	};

	static std::unordered_set<ExpectedType, EnumClassHash> all_allowed = {
		ExpectedType::KEY,
		ExpectedType::VALUE,
		ExpectedType::START_DICT,
		ExpectedType::START_ARRAY,
		ExpectedType::END_ARRAY
	};

	class Builder {
		class BaseContext;
		class KeyContext;
		class DictContext;
		class ArrayContext;

	public:
		Builder();

		KeyContext Key(std::string str);
		Builder& Value(Node::Value val);
		DictContext StartDict();
		ArrayContext StartArray();
		Builder& EndDict();
		Builder& EndArray();
		json::Node Build();

	private:
		std::string GenerateTextError() const;
		void CheckExpected(ExpectedType type) const;
		Node GetNodeFromValue(Node::Value&& val) const;

		Node root_;
		std::vector<Node*> nodes_stack_;
		std::unordered_set<ExpectedType, EnumClassHash> expected_;
		std::optional<std::string> current_key_;

		class BaseContext {
		public:
			BaseContext(Builder& builder) : builder_(builder) {}

			KeyContext Key(std::string str) {
				return builder_.Key(std::move(str));
			}

			Builder& Value(Node::Value val) {
				return builder_.Value(std::move(val));
			}

			DictContext StartDict() {
				return builder_.StartDict();
			}

			ArrayContext StartArray() {
				return builder_.StartArray();
			}

			Builder& EndDict() {
				return builder_.EndDict();
			}

			Builder& EndArray() {
				return builder_.EndArray();
			}

		protected:
			Builder& builder_;
		};

		class KeyContext : public BaseContext {
		public:
			KeyContext(BaseContext base) : BaseContext(base) {}

			KeyContext Key(std::string str) = delete;
			Builder& EndDict() = delete;
			Builder& EndArray() = delete;

			DictContext Value(Node::Value val) {
				builder_.Value(std::move(val));
				return DictContext(*this);
			}
		};

		class DictContext : public BaseContext {
		public:
			DictContext(BaseContext base) : BaseContext(base) {}



			DictContext StartDict() = delete;
			ArrayContext StartArray() = delete;
			Builder& EndArray() = delete;

			DictContext Value(Node::Value val) = delete;
		};

		class ArrayContext : public BaseContext {
		public:
			ArrayContext(BaseContext base) : BaseContext(base) {}

			KeyContext Key(std::string str) = delete;
			Builder& EndDict() = delete;

			ArrayContext Value(Node::Value val) {
				builder_.Value(std::move(val));
				return ArrayContext(*this);
			}
		};


	};

}	// namespace json