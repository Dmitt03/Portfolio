#include "player.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

using namespace app;
using namespace model;
using namespace collision_detector;
using namespace postgres;

const double DELTA = 0.4;

// -------------------------- Player ------------------------------

Player::Player(GameSession* game_session_ptr, model::Dog* dog_ptr)
	: game_session_ptr_(game_session_ptr), dog_ptr_(dog_ptr) {
}

GameSession* Player::GetSessionPtr() {
	return game_session_ptr_;
}

uint64_t Player::GetDogId() const {
	return dog_ptr_->GetId();
}

model::Dog* Player::GetDogPtr() {
	return dog_ptr_;
}

void Player::SetToken(const Token& token) {
	token_ = token;
}

Token Player::GetToken() const {
	return token_.value();
}

// ------------------------ PlayerTokens ---------------------------

Token PlayerTokens::GenerateToken() {
	std::uniform_int_distribution<std::mt19937_64::result_type> dist;
	const auto a = dist(generator1_);
	const auto b = dist(generator2_);

	std::ostringstream oss;
	oss << std::hex << std::nouppercase
		<< std::setw(16) << std::setfill('0') << static_cast<uint64_t>(a)
		<< std::setw(16) << std::setfill('0') << static_cast<uint64_t>(b);

	return Token(oss.str());
}


Player* PlayerTokens::FindPlayerByToken(const Token& token) const {
	auto it = token_to_player_.find(token);
	return (it == token_to_player_.end()) ? nullptr : it->second;
}

Token PlayerTokens::AddPlayer(Player& player) {
	auto token = GenerateToken();
	while (token_to_player_.contains(token)) {
		token = GenerateToken();
	}

	// Вставляем в map и сразу берём итератор на ключ
	token_to_player_.emplace(token, &player);

	return token;
}

void PlayerTokens::DeletePlayer(const Token& token) {
	token_to_player_.erase(token);
}

// ------------------- GameSession -------------------

static void NormalizeIntervals(std::vector<RoadInterval>& intervals) {
	// 1. сортируем по левому краю a (если равен, то по b)
	std::sort(intervals.begin(), intervals.end(),
		[](const RoadInterval& lhs, const RoadInterval& rhs) {
			if (lhs.a != rhs.a) return lhs.a < rhs.a;
			return lhs.b < rhs.b;
		}
	);

	// 2. сливаем пересечения/контакт
	std::vector<RoadInterval> merged;
	for (const auto& seg : intervals) {
		if (merged.empty() || seg.a > merged.back().b) {
			// новый непересекающийся сегмент
			merged.push_back(seg);
		}
		else {
			// пересечение или касание: расширяем последний
			if (seg.b > merged.back().b) {
				merged.back().b = seg.b;
			}
		}
	}
	intervals.swap(merged);
}

GameSession::GameSession(const model::Map* map_ptr, bool is_random_dog_position) : 
	map_ptr_(map_ptr), is_random_dog_position_(is_random_dog_position){
	// подготовим интервалы
	for (const Road& road : map_ptr->GetRoads()) {
		auto road_start = road.GetStart();
		auto road_end = road.GetEnd();
		if (road.IsHorizontal()) {	// значит y константа
			if (road_start.x <= road_end.x) {
				horizontal_intervals_[road_start.y].emplace_back(road_start.x - DELTA, road_end.x + DELTA);
			}
			else {
				horizontal_intervals_[road_start.y].emplace_back(road_end.x - DELTA, road_start.x + DELTA);
			}
		}
		else {	// vertical
			if (road_start.y <= road_end.y) {
				vertical_intervals_[road_start.x].emplace_back(road_start.y - DELTA, road_end.y + DELTA);
			}
			else {
				vertical_intervals_[road_start.x].emplace_back(road_end.y - DELTA, road_start.y + DELTA);
			}
		}
	}

	// НОРМАЛИЗУЕМ
	for (auto& [y, intervals] : horizontal_intervals_) {
		NormalizeIntervals(intervals);
	}
	for (auto& [x, intervals] : vertical_intervals_) {
		NormalizeIntervals(intervals);
	}
}

Dog* GameSession::AddDogToMap(model::Dog dog) {
	dog.SetId(local_id++);

	if (!is_random_dog_position_) {
		auto start_pos = RealCoord(map_ptr_->GetRoads()[0].GetStart().x, map_ptr_->GetRoads()[0].GetStart().y);
		dog.SetPosition(RealCoord(map_ptr_->GetRoads()[0].GetStart().x, map_ptr_->GetRoads()[0].GetStart().y));
	}
	else {
		dog.SetPosition(GenerateRandomPosition());	// рандомно ставим
	}

	dogs_.push_back(std::move(dog));
	return &dogs_.back();
}

const model::Map* GameSession::GetMapPtr() const {
	return map_ptr_;
}

void GameSession::SetMapPtr(model::Map* map_ptr) {
	map_ptr_ = map_ptr;
}

std::deque<Dog>& GameSession::GetDogs() {
	return dogs_;
}

model::RealCoord GameSession::GenerateRandomPosition() const {
	// Если по какой-то причине нет карты или дорог
	if (!map_ptr_) {
		return model::RealCoord{ 0.0, 0.0 };
	}

	const auto& roads = map_ptr_->GetRoads();
	if (roads.empty()) {
		return model::RealCoord{ 0.0, 0.0 };
	}

	// thread_local, чтобы не дергать std::random_device каждый раз
	static thread_local std::mt19937_64 rng{ std::random_device{}() };

	// выбираем случайную дорогу
	std::uniform_int_distribution<size_t> pick_road(0, roads.size() - 1);
	const model::Road& road = roads[pick_road(rng)];

	const model::Point a = road.GetStart();
	const model::Point b = road.GetEnd();

	double spawn_x = 0.0;
	double spawn_y = 0.0;

	if (road.IsHorizontal()) {
		// дорога идёт от (x0, y) до (x1, y)
		int x_min = std::min(a.x, b.x);
		int x_max = std::max(a.x, b.x);

		std::uniform_real_distribution<double> pick_x(
			static_cast<double>(x_min),
			static_cast<double>(x_max)
		);

		spawn_x = pick_x(rng);
		spawn_y = static_cast<double>(a.y); // y фиксированный
	}
	else {
		// дорога вертикальная: от (x, y0) до (x, y1)
		int y_min = std::min(a.y, b.y);
		int y_max = std::max(a.y, b.y);

		std::uniform_real_distribution<double> pick_y(
			static_cast<double>(y_min),
			static_cast<double>(y_max)
		);

		spawn_y = pick_y(rng);
		spawn_x = static_cast<double>(a.x); // x фиксированный
	}

	return model::RealCoord{ spawn_x, spawn_y };
}

void GameSession::DeleteDog(const Dog* dog_ptr) {
	auto it = std::find_if(dogs_.begin(), dogs_.end(), [dog_ptr](const Dog& d) {
		return &d == dog_ptr;
		});
	dogs_.erase(it);
}

RoadInterval GameSession::GetHorizontalInterval(const Dog& dog) const {
	RealCoord dog_pos = dog.GetPosition();
	RoadInterval alowed_interval;
	int rounded_y = std::lround(dog_pos.GetY());
	auto buckets_it = horizontal_intervals_.find(rounded_y);
	double difference = std::abs(dog_pos.GetY() - rounded_y);

	if (buckets_it == horizontal_intervals_.end() || difference > DELTA) {
		int rounded_x = std::lround(dog_pos.GetX());
		alowed_interval.a = rounded_x - DELTA;
		alowed_interval.b = rounded_x + DELTA;
		return alowed_interval;
	}

	const auto& intervals = buckets_it->second;
	auto it = std::upper_bound(
		intervals.begin(), intervals.end(), dog_pos.GetX(),
		[](double x, const RoadInterval& interval) {
			return x < interval.a;
		});

	if (it == intervals.begin()) {
		return *it;
	}
	if (it == intervals.end()) {
		return intervals.back();
	}
	--it;
	
	if (dog_pos.GetX() < it->a || dog_pos.GetX() > it->b) {
		int rounded_x = std::lround(dog_pos.GetX());
		RoadInterval allowed{ rounded_x - DELTA, rounded_x + DELTA };
		return allowed;
	}
	return *it;
}

RoadInterval GameSession::GetVerticalInterval(const Dog& dog) const {
	RealCoord dog_pos = dog.GetPosition();
	RoadInterval alowed_interval;
	int rounded_x = std::lround(dog_pos.GetX());
	auto buckets_it = vertical_intervals_.find(rounded_x);
	double difference = std::abs(dog_pos.GetX() - rounded_x);

	if (buckets_it == vertical_intervals_.end() || difference > DELTA) {		
		int rounded_y = std::lround(dog_pos.GetY());
		alowed_interval.a = rounded_y - DELTA;
		alowed_interval.b = rounded_y + DELTA;
		return alowed_interval;
	}

	const auto& intervals = buckets_it->second;
	auto it = std::upper_bound(
		intervals.begin(), intervals.end(), dog_pos.GetY(),
		[](double y, const RoadInterval& interval) {
			return y < interval.a;
		});

	if (it == intervals.begin()) {
		return *it;
	}
	if (it == intervals.end()) {
		return intervals.back();
	}
	--it;

	if (dog_pos.GetY() < it->a || dog_pos.GetY() > it->b) {
		int rounded_y = std::lround(dog_pos.GetY());
		RoadInterval allowed{ rounded_y - DELTA, rounded_y + DELTA };
		return allowed;
	}
	return *it;
}

// вернём пару начальная позиция - конечная
std::pair<RealCoord, RealCoord> GameSession::CalculatePosition(Dog& dog, int milliseconds) {
	Direction direction = dog.GetDirection();
	double coef = milliseconds / 1000.0;	// поправил преобразование
	RealCoord dog_speed = dog.GetSpeed();
	RealCoord dog_start_position = dog.GetPosition();

	RoadInterval interval;

	if (direction == Direction::NORTH || direction == Direction::SOUTH) {
		interval = GetVerticalInterval(dog);

		double with_offset_y = dog_start_position.GetY() + dog_speed.GetY() * coef;

		if (direction == Direction::NORTH) {
			// вверх => y уменьшается => упираемся в левую границу интервала [a, b]
			if (with_offset_y < interval.a) {
				with_offset_y = interval.a;
				dog.StopDog();
			}
		}
		else { // SOUTH
			// вниз => y растёт => упираемся в правую границу
			if (with_offset_y > interval.b) {
				with_offset_y = interval.b;
				dog.StopDog();
			}
		}

		dog.SetPosition(dog_start_position.GetX(), with_offset_y);		
	}
	if (direction == Direction::WEST || direction == Direction::EAST) {
		interval = GetHorizontalInterval(dog);

		double with_offset_x = dog_start_position.GetX() + dog_speed.GetX() * coef;
		if (direction == Direction::EAST) {
			// идём направо по иксу, ограничение b]
			if (with_offset_x > interval.b) {
				with_offset_x = interval.b;
				dog.StopDog();
			}
		}
		else {	// west
			// идём налево, с ограничением [a
			if (with_offset_x < interval.a) {
				with_offset_x = interval.a;
				dog.StopDog();
			}
		}
		// перемещались только по x, поэтому y оставляем старым
		dog.SetPosition(with_offset_x, dog_start_position.GetY());		
	}
	RealCoord dog_end_position = dog.GetPosition();
	return { dog_start_position, dog_end_position };
}

std::vector<std::pair<RealCoord, RealCoord>> GameSession::ProcessTickMove(int milliseconds) {
	std::vector<std::pair<RealCoord, RealCoord>> result;
	for (Dog& dog : dogs_) {
		std::pair<RealCoord, RealCoord> current_move = CalculatePosition(dog, milliseconds);
		result.push_back(current_move);
	}
	return result;
}

void GameSession::AddLostObject(model::LostObject lost_object) {
	lost_objects_.push_back(std::move(lost_object));
}

std::vector<model::LostObject>& GameSession::GetLostObjects() {
	return lost_objects_;
}

// ---------------------- Players ----------------------------------

Player& Players::AddDogToSession(std::string dog_name, GameSession& session) {
	Dog dog(std::move(dog_name));
	model::Dog* dog_ptr = session.AddDogToMap(std::move(dog));
	// для последующего нахождения игрока по ключу
	PlayerKey pk{ .map_id = session.GetMapPtr()->GetId(), .dog_id = dog_ptr->GetId() };
	storage_.emplace_back(&session, dog_ptr);
	Player& added_player = storage_.back();
	player_ptr_by_key_[std::move(pk)] = &added_player;
	return added_player;
}

Player* Players::FindByDogIdAndMapId(uint64_t dog_id, model::Map::Id id) const {
	if (auto it = player_ptr_by_key_.find(PlayerKey{ id, dog_id }); it != player_ptr_by_key_.end()) {
		return it->second;
	}
	return nullptr;
}

void Players::DeletePlayer(uint64_t dog_id, model::Map::Id id) {
	Player* player_to_delete_ptr = FindByDogIdAndMapId(dog_id, id);
	player_ptr_by_key_.erase(PlayerKey{ .map_id = id, .dog_id = dog_id });
	auto it = std::find_if(storage_.begin(), storage_.end(), [player_to_delete_ptr] (const Player& p) {
		return &p == player_to_delete_ptr;
	});
	storage_.erase(it);
}

// --------------------------- GameSessionManager -------------------------------

GameSession* GameSessionManager::GetSessionByMapId(const model::Map::Id& id) {
	auto it = map_id_to_session_.find(id);
	if (it == map_id_to_session_.end()) {
		return nullptr;
	}
	return it->second;
}

GameSession* GameSessionManager::SelectSession(const Map::Id& map_id) {
	GameSession* session_ptr = GetSessionByMapId(map_id);
	if (!session_ptr) {
		// если сессии нет - создадим её
		const Map* map_ptr = game_.FindMap(map_id);
		if (!map_ptr) {
			throw std::runtime_error("eternal error. Map not found");
		}
		GameSession new_session(map_ptr, is_random_dog_position_);

		// индексируем
		sessions_.push_back(std::move(new_session));
		GameSession* session_ptr = &sessions_.back();
		map_id_to_session_[map_id] = session_ptr;
		return session_ptr;
	}
	return session_ptr;
}


// вторым параметром будет идти dog id
std::pair<Token, uint64_t> GameSessionManager::AddDogToMap(std::string name, model::Map::Id map_id) {
	GameSession* session = SelectSession(map_id);

	Player& player = players_.AddDogToSession(std::move(name), *session);

	Token token = player_tokens_.AddPlayer(player);

	return { token, player.GetDogId() };
}


Player* GameSessionManager::FindPlayerByToken(Token token) {
	return player_tokens_.FindPlayerByToken(token);
}

Direction GameSessionManager::GetConvertedDirection(std::string_view dir) {
	if (dir == "U") return Direction::NORTH;
	if (dir == "D") return Direction::SOUTH;
	if (dir == "L") return Direction::WEST;
	if (dir == "R") return Direction::EAST;
	return Direction::NONE; // сработает если пустая
}

void GameSessionManager::SetMoveDog(Player* dog_owner, std::string_view command) {
	Direction dir = GetConvertedDirection(command);
	// узнаём скорость собаки для конкретной карты
	double speed = dog_owner->GetSessionPtr()->GetMapPtr()->GetDogSpeed();
	dog_owner->GetDogPtr()->SetMoveDog(dir, speed);
}

void GameSessionManager::GenerateLoot(GameSession& session, int ms) {
	using loot_gen::LootGenerator;

	LootGenerator::TimeInterval delta{ ms };

	const auto loot_count = session.GetLostObjects().size();
	const auto looter_count = session.GetDogs().size();
	const auto loot_types_count = loot_map_.at(session.GetMapPtr()->GetId()).size();

	// Нет собак или нет типов лута — ничего не генерируем
	if (looter_count == 0 || loot_types_count == 0) {
		return;
	}

	const unsigned generates = loot_gen_.Generate(
		delta,
		static_cast<unsigned>(loot_count),
		static_cast<unsigned>(looter_count)
	);

	static thread_local std::mt19937_64 rng{ std::random_device{}() };
	std::uniform_int_distribution<size_t> pick_type(0, loot_types_count - 1);

	for (unsigned i = 0; i < generates; ++i) {
		int random_type = static_cast<int>(pick_type(rng));

		model::LostObject lo{
			.type = random_type,
			.pos = session.GenerateRandomPosition()
		};

		session.AddLostObject(std::move(lo));
	}
}

const double PLAYER_RADIUS = 0.6 / 2;
const double ITEM_RADIUS = 0;
const double BASE_RADIUS = 0.5 / 2;

void GameSessionManager::ProcessGatherEvent(GameSession& session, const std::vector<std::pair<model::RealCoord, model::RealCoord>>& all_moves) {
	ItemGatherer item_gatherer;
	// сначала добавим все собирателей 
	for (const auto& [dog_id, move] : all_moves) {
		item_gatherer.AddGatherer(Gatherer{ dog_id, move, PLAYER_RADIUS });
	}
	// теперь сначала добавим лут на карте, индекс в сессии будет совпадать с индексами предметов здесь прямо до size
	for (const auto& item : session.GetLostObjects()) {
		item_gatherer.AddItem(Item{ item.pos, ITEM_RADIUS });
	}
	// сразу после потерянных предметов будут лежать базы
	for (const auto& office : session.GetMapPtr()->GetOffices()) {
		item_gatherer.AddItem(Item{
			RealCoord(office.GetPosition().x, office.GetPosition().y),
			BASE_RADIUS
			});
	}
	std::vector<GatheringEvent> gathering_events = FindGatherEvents(item_gatherer);
	// начнём сбор
	for (const auto& gathering_event : gathering_events) {

		// получаем реального игрока по индексу
		Dog& current_dog = session.GetDogs().at(gathering_event.gatherer_id);

		// если это база (базы лежат после всех объектов, то сбрасываем рюкзак)
		if (gathering_event.item_id >= session.GetLostObjects().size()) {
			// отсюда будем брать очки за тип лута
			std::vector<extra_data::LootType>& objects_with_values = loot_map_.at(session.GetMapPtr()->GetId());
			std::vector<model::BagItem> released_loot = current_dog.ReleaseLootFromBag();

			for (const auto exchanging_object : released_loot) {
				// по индексу находим стоимость
				int current_object_value = objects_with_values.at(exchanging_object.type).GetValue();			
				current_dog.AddScore(current_object_value);
			}

			continue;
		}

		// получаем реальный объект по индексу
		LostObject& lo = session.GetLostObjects().at(gathering_event.item_id);
		if (lo.is_collected) {
			continue;
		}

		// проверяем, есть ли место в рюкзаке
		int bag_size = session.GetMapPtr()->GetBagCapacity();
		if (current_dog.GetLootInBag().size() == bag_size) {
			continue;
		}

		// если все круги ада прошли, можно со спокойной душой класть предмет в рюкзак
		current_dog.AddLootToBag(model::BagItem{gathering_event.item_id, lo.type});
		// не забываем пометить его как подобранный
		lo.is_collected = true;
	}
	// теперь очистим карту от подобранных вещей
	auto& lost_objects = session.GetLostObjects();
	lost_objects.erase(
		std::remove_if(lost_objects.begin(), lost_objects.end(), [](const LostObject l) {
			return l.is_collected == true;
			}),
		lost_objects.end()
	);
} 

void GameSessionManager::SetListener(ApplicationListener* listener) {
	listener_ = listener;
}

std::vector<RetiredRecord> GameSessionManager::GetRecords(std::size_t start,
	std::size_t max_items) const {	
	return repo_.Get(static_cast<int>(start), static_cast<int>(max_items));
}


void GameSessionManager::CheckAfk(GameSession& session, int time_in_ms) {
	const auto dt = std::chrono::milliseconds{ time_in_ms };

	struct RetireInfo {
		uint64_t dog_id;
		model::Map::Id map_id;
	};
	std::vector<RetireInfo> to_retire;
	
	for (model::Dog& dog : session.GetDogs()) {
		dog.AddPlayTime(dt);

		if (dog.GetSpeed() == model::RealCoord{ 0, 0 }) {
			dog.AddAfkTime(dt);

			if (dog.GetAfkTime() >= retirement_time_) {
				to_retire.push_back({ dog.GetId(), session.GetMapPtr()->GetId() });
			}
		}
		else {
			dog.ResetAfkTime();
		}
	}

	
	for (const auto& [dog_id, map_id] : to_retire) {
		Player* p = players_.FindByDogIdAndMapId(dog_id, map_id);
		if (!p) continue;

		// всё нужное сохранили до удаления
		model::Dog* dog_ptr = p->GetDogPtr();

		// play_time должен быть double
		const double play_time = dog_ptr->GetPlayTimeSec();
		const int score = dog_ptr->GetScore();
		std::string name(dog_ptr->GetName());

		// токен надо удалить из token_to_player
		const Token tok = p->GetToken(); 
		player_tokens_.DeletePlayer(tok);

		// запись в БД
		repo_.Add(postgres::RetiredRecord{
			.name = std::move(name),
			.score = score,
			.play_time = play_time
			});

		// удалить из players_ и из session
		players_.DeletePlayer(dog_id, map_id);
		session.DeleteDog(dog_ptr);
	}
}

void GameSessionManager::ProcessTick(int ms) {
	for (GameSession& session : sessions_) {		
		GenerateLoot(session, ms);
		std::vector<std::pair<RealCoord, RealCoord>> all_moves = session.ProcessTickMove(ms);
		ProcessGatherEvent(session, all_moves);
		CheckAfk(session, ms);
	}
	listener_->OnTick(std::chrono::milliseconds(ms));
}

boost::json::array GameSessionManager::GetSerializedLostObjectByMapId(model::Map::Id map_id) const {
	// для каждой карты свой вектор потерянных предметов
	auto& loot_vector = loot_map_.at(map_id);
	boost::json::array result_arr;
	for (auto& item : loot_vector) {
		result_arr.push_back(item.GetAsJsonObject());
	}
	return result_arr;	// если поля были распарсены, понадобится обратная сборка, пока промежуточный вариант
}

