#pragma once

#include "retire_repository.h"

namespace postgres {

class RetiredPlayersRepositoryImpl : public RetiredPlayersRepository {
public:
    explicit RetiredPlayersRepositoryImpl(ConnectionPool& pool) : pool_(pool) {}

    void EnsureSchema() override {
        auto conn = pool_.GetConnection();
        pqxx::work w(*conn);
        w.exec(R"(CREATE TABLE IF NOT EXISTS retired_players(
                id BIGSERIAL PRIMARY KEY,
                name TEXT NOT NULL,
                score INTEGER NOT NULL,
                play_time DOUBLE PRECISION NOT NULL
                );)");
        w.exec(R"(CREATE INDEX IF NOT EXISTS retired_players_sort_idx
                ON retired_players (score DESC, play_time ASC, name ASC);)");
        w.commit();
    }
    void Add(const RetiredRecord& r) override {
        auto conn = pool_.GetConnection();
        pqxx::work w(*conn);
        w.exec_params("INSERT INTO retired_players(name, score, play_time) VALUES ($1,$2,$3)",
            r.name, r.score, r.play_time);
        w.commit();
    }

    std::vector<RetiredRecord> Get(int start, int max_items) override {
        auto conn = pool_.GetConnection();
        pqxx::read_transaction tr(*conn);
        auto res = tr.exec_params(
            "SELECT name, score, play_time FROM retired_players "
            "ORDER BY score DESC, play_time ASC, name ASC "
            "OFFSET $1 LIMIT $2",
            start, max_items
        );

        std::vector<RetiredRecord> out;
        out.reserve(res.size());
        for (auto row : res) {
            out.push_back(RetiredRecord{
                row[0].c_str(),
                row[1].as<int>(),
                row[2].as<double>()
                });
        }
        return out;
    }

private:
    ConnectionPool& pool_;
};

}