// 数据库测试 (shùjùkù cèshì — database test)
#include "db.h"
#include <iostream>
#include <cassert>

int main() {
    Db db(":memory:");  // in-memory DB for testing
    assert(db.ok());

    // ── User tests ──
    assert(db.create_user("alice@test.com", "hash123"));
    assert(db.create_user("bob@test.com", "hash456"));

    UserRow* u = db.find_user_by_email("alice@test.com");
    assert(u);
    assert(u->email == "alice@test.com");
    assert(u->password_hash == "hash123");
    delete u;

    u = db.find_user_by_id(3);
    assert(u);
    assert(u->email == "bob@test.com");
    delete u;

    assert(db.find_user_by_email("nobody@test.com") == nullptr);

    // ── Token tests ──
    assert(db.save_token("tok1", 1));
    assert(db.save_token("tok2", 2));
    assert(db.find_user_by_token("tok1") == 1);
    assert(db.find_user_by_token("tok2") == 2);
    assert(db.find_user_by_token("bad") == -1);

    db.delete_token("tok1");
    assert(db.find_user_by_token("tok1") == -1);

    // ── Room tests ──
    int room_id = db.create_room("ABCD", 1);
    assert(room_id > 0);

    RoomRow* r = db.find_room_by_code("ABCD");
    assert(r);
    assert(r->code == "ABCD");
    assert(r->host_id == 1);
    assert(r->status == "waiting");
    delete r;

    // ── Room player tests ──
    assert(db.add_room_player(room_id, 1));
    assert(db.add_room_player(room_id, 2));

    auto players = db.get_room_players(room_id);
    assert(players.size() == 2);
    assert(players[0].ready == false);

    assert(db.set_player_ready(room_id, 1, true));
    players = db.get_room_players(room_id);
    for (auto& p : players) {
        if (p.user_id == 1) assert(p.ready == true);
    }

    db.remove_room_player(room_id, 2);
    players = db.get_room_players(room_id);
    assert(players.size() == 1);

    assert(db.update_room_status(room_id, "playing"));
    r = db.find_room_by_code("ABCD");
    assert(r && r->status == "playing");
    delete r;

    db.delete_room(room_id);
    assert(db.find_room_by_code("ABCD") == nullptr);

    cout << "[DB] All tests passed!" << endl;
    return 0;
}
