#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

CollectionResult TryCollectPoint(model::RealCoord a, model::RealCoord b, model::RealCoord c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // пскольку при сборе заказов придётся учитывать перемещение даже на небольшое
    // расстояние.
    assert(b.GetX() != a.GetX() || b.GetY() != a.GetY());
    const double u_x = c.GetX() - a.GetX();
    const double u_y = c.GetY() - a.GetY();
    const double v_x = b.GetX() - a.GetX();
    const double v_y = b.GetY() - a.GetY();
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}


std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> events;

    const size_t items_count = provider.ItemsCount();
    const size_t gatherers_count = provider.GatherersCount();

    // Перебираем всех собирателей и все предметы
    for (size_t g_id = 0; g_id < gatherers_count; ++g_id) {
        Gatherer g = provider.GetGatherer(g_id);

        // Если собиратель не двигался — столкновений не ищем
        if (g.start_pos.GetX() == g.end_pos.GetX() && g.start_pos.GetY() == g.end_pos.GetY()) {
            continue;
        }

        for (size_t i_id = 0; i_id < items_count; ++i_id) {
            Item item = provider.GetItem(i_id);

            // Радиус сбора это сумма радиусов
            const double collect_radius = g.width + item.width;

            // Считаем расстояние и относительное время
            CollectionResult res = TryCollectPoint(g.start_pos, g.end_pos, item.position);

            if (res.IsCollected(collect_radius)) {
                GatheringEvent ev;
                ev.item_id = i_id;
                ev.gatherer_id = g_id;
                ev.sq_distance = res.sq_distance;
                ev.time = res.proj_ratio; // относительное время [0,1] вдоль отрезка

                events.emplace_back(ev);
            }
        }
    }

    // Сортируем события в хронологическом порядке
    std::sort(events.begin(), events.end(),
        [](const GatheringEvent& lhs, const GatheringEvent& rhs) {
            if (lhs.time != rhs.time) {
                return lhs.time < rhs.time;
            }
            // Для детерминизма при равном времени
            if (lhs.gatherer_id != rhs.gatherer_id) {
                return lhs.gatherer_id < rhs.gatherer_id;
            }
            return lhs.item_id < rhs.item_id;
        });

    return events;
}


}  // namespace collision_detector