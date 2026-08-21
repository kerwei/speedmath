// 数据库测试 (shùjùkù cèshì — database test)
#include "db.h"
#include <iostream>
#include <cassert>

int main() {
    // 邮箱加密需要 enc_key (yóuxiāng jiāmì xūyào — email crypto needs the key)
    crypto::set_enc_key(string(64, 'a'));

    Db db(":memory:");  // in-memory DB for testing
    assert(db.ok());
    db.migrate();       // 种子用户 + 迁移 (zhǒngzǐ yònghù + qiānyí — seed users + migration)

    // ── User tests ──
    assert(db.create_user(email_enc_of("alice@test.com"), email_hash_of("alice@test.com"), "hash123", "alice"));
    assert(db.create_user(email_enc_of("bob@test.com"), email_hash_of("bob@test.com"), "hash456", "bob"));

    UserRow* u = db.find_user_by_email_hash(email_hash_of("alice@test.com"));
    assert(u);
    assert(u->username == "alice");
    assert(u->password_hash == "hash123");
    assert(u->username_updated_at.empty());          // 未自行设置 (wèi zìxíng shèzhì — never self-chosen)
    assert(crypto::aes_decrypt(u->email_enc) == "alice@test.com");  // 加密可还原 (jiāmì kě huányuán — decrypts back)
    delete u;

    // Resolve bob's actual id (seed users occupy ids 1-5, so ids are not fixed)
    u = db.find_user_by_email_hash(email_hash_of("bob@test.com"));
    assert(u);
    int bob_id = u->id;
    delete u;

    u = db.find_user_by_id(bob_id);
    assert(u);
    assert(u->username == "bob");
    delete u;

    assert(db.find_user_by_email_hash(email_hash_of("nobody@test.com")) == nullptr);

    // 邮箱规范化: 大小写 + 空格不影响查找 (guīfàn huà — case/space-insensitive lookup)
    assert(db.find_user_by_email_hash(email_hash_of("  Alice@Test.com ")) != nullptr);

    // ── Username tests ──
    assert(db.username_exists("alice"));
    assert(!db.username_exists("nobody"));

    assert(db.update_username(bob_id, "bobby", "2026-01-01 00:00:00"));
    u = db.find_user_by_id(bob_id);
    assert(u);
    assert(u->username == "bobby");
    assert(u->username_updated_at == "2026-01-01 00:00:00");
    delete u;

    // 随机用户名: 唯一且以 player 开头 (suíjī yònghùmíng — unique, prefixed)
    string rnd = db.generate_random_username();
    assert(!rnd.empty());
    assert(rnd.rfind("player", 0) == 0);
    assert(!db.username_exists(rnd));

    // ── Password update ──
    assert(db.update_password(bob_id, "newhash"));
    u = db.find_user_by_id(bob_id);
    assert(u && u->password_hash == "newhash");
    delete u;

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
    // 玩家列表现在返回 username 而不是邮箱 (fǎnhuí username — returns username, not email)
    for (auto& p : players) assert(!p.username.empty());

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
