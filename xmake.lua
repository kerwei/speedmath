-- 项目配置 (xiàngmù pèizhì — project configuration)
set_xmakever("3.0.0")
set_project("quickmath")
set_version("1.0.0")
set_languages("c++20")

-- 静态库 (jìngtài kù — static library): 核心游戏逻辑 (héxīn yóuxì luóji — core game logic)
target("libquickmath")
    set_kind("static")
    add_files("libquickmath/*.cpp")
    add_includedirs(".")

-- 后端服务器 (hòuduān fúwùqì — backend server)
target("backend")
    set_kind("binary")
    add_files("backend/main.cpp", "backend/crypto.cpp", "backend/unicode.cpp")
    add_deps("libquickmath")
    add_includedirs("backend")
    add_syslinks("sqlite3", "crypto", "ssl")
    -- 静态链接 ICU + libstdc++/libgcc (2026-08-18 教训): 生产服务器 (AL4) 的 ICU 主版本
    -- 与 Ubuntu 构建机不同, 动态链接 libicuuc.so.N 会因 SONAME 不匹配而无法启动;
    -- libstdc++/libgcc 静态化避免 GLIBCXX 版本不匹配。静态化后 DT_NEEDED 仅剩
    -- sqlite3/crypto/ssl (服务器均已具备) + glibc。
    -- {force=true}: xmake 的 check.auto_ignore_flags 会静默丢弃 -Wl,-Bstatic
    add_ldflags("-static-libstdc++", "-static-libgcc")
    add_ldflags("-Wl,-Bstatic", "-licuuc", "-licudata", "-Wl,-Bdynamic", {force = true})

-- 可执行文件 (kě zhíxíng wénjiàn — executable binary): 控制台入口 (kòngzhìtái rùkǒu — console entry point)
target("quickmath")
    set_kind("binary")
    add_files("main.cpp")
    add_deps("libquickmath")

-- 一次性数据库迁移工具 (yīcìxìng qiānyí gōngjù — one-time v1→v2 migration tool)
-- 仅部署/开发时运行一次, 之后可整体退役 (runs once at deploy; retirable later)
target("db_migrate")
    set_kind("binary")
    add_files("backend/migrate_v1.cpp", "backend/crypto.cpp")
    add_includedirs("backend")
    add_syslinks("sqlite3", "crypto", "ssl")
    -- 与 backend 一致: 静态 libstdc++/libgcc (2026-08-18 GLIBCXX 事故)
    add_ldflags("-static-libstdc++", "-static-libgcc")

-- 测试 (cèshì — tests): runs all unit tests
target("tests")
    set_kind("binary")
    add_files("tests/test_*.cpp")
    add_files("backend/crypto.cpp", "backend/unicode.cpp")
    add_deps("libquickmath")
    add_includedirs("tests", "backend")
    add_syslinks("crypto", "ssl", "icuuc")

-- 数据库测试 (shùjùkù cèshì — database test)
target("test_db")
    set_kind("binary")
    add_files("tests/db_test.cpp", "backend/crypto.cpp")
    add_includedirs("backend")
    add_syslinks("sqlite3", "crypto", "ssl")
