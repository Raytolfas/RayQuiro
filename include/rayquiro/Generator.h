#pragma once

#include <fstream>
#include <cctype>
#include <unordered_map>
#include <sstream>
#include <string>
#include <vector>

#include "AST.h"
#include "RuntimeEmitter.h"

class Generator {
    std::ofstream out;
    RuntimeFeatures features;
    std::unordered_map<std::string, std::string> builtinNamespaceAliases;
    std::unordered_map<std::string, std::string> builtinSymbolAliases;
    std::unordered_map<std::string, FunctionStmt*> userFunctionMap_;
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> structDefaults_;
    int tempVarCounter_ = 0;

    std::string escapeString(const std::string& value) const {
        std::string escaped;
        for (char c : value) {
            if (c == '\\') escaped += "\\\\";
            else if (c == '"') escaped += "\\\"";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\t') escaped += "\\t";
            else if (c == '\r') escaped += "\\r";
            else escaped += c;
        }
        return escaped;
    }

    std::string canonicalizeBuiltinName(const std::string& name) const {
        const auto exact = builtinSymbolAliases.find(name);
        if (exact != builtinSymbolAliases.end()) {
            return exact->second;
        }

        const size_t dot = name.find('.');
        if (dot == std::string::npos) {
            return name;
        }

        const std::string prefix = name.substr(0, dot);
        const auto found = builtinNamespaceAliases.find(prefix);
        if (found == builtinNamespaceAliases.end()) {
            return name;
        }

        return found->second + name.substr(dot);
    }

    std::string mangleSymbol(const std::string& name) const {
        if (name.rfind("_rq_", 0) == 0) return name;
        std::string mangled = "_rq_u_";
        mangled.reserve(name.size() + 14);

        for (char c : name) {
            if (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_') {
                mangled += c;
            } else if (c == '.') {
                mangled += "__rqm__";
            } else {
                mangled += '_';
            }
        }

        return mangled;
    }

    void scanExpr(Expr* expr) {
        if (!expr) return;

        if (auto call = dynamic_cast<CallExpr*>(expr)) {
            const std::string callee = canonicalizeBuiltinName(call->callee);
            if (callee.rfind("app.", 0) == 0) features.usesApp = true;
            if (callee.rfind("ui.", 0) == 0) features.usesUi = true;
            if (callee.rfind("web.", 0) == 0) features.usesWeb = true;
            if (callee.rfind("engine.", 0) == 0) features.usesEngine = true;
            if (callee.rfind("fs.", 0) == 0) features.usesFs = true;
            if (callee.rfind("env.", 0) == 0) features.usesEnv = true;
            if (callee.rfind("process.", 0) == 0) features.usesProcess = true;
            if (callee.rfind("os.", 0) == 0) features.usesOs = true;
            if (callee.rfind("net.", 0) == 0) features.usesNet = true;
            if (callee.rfind("http.", 0) == 0) features.usesHttp = true;
            if (callee.rfind("db.", 0) == 0) features.usesDb = true;
            if (builtinName(call->callee).empty() && callee.find('.') != std::string::npos) {
                const std::string prefix = callee.substr(0, callee.find('.'));
                if (prefix != "app" &&
                    prefix != "ui" &&
                    prefix != "web" &&
                    prefix != "engine" &&
                    prefix != "fs" &&
                    prefix != "env" &&
                    prefix != "process" &&
                    prefix != "os" &&
                    prefix != "net" &&
                    prefix != "http" &&
                    prefix != "db" &&
                    prefix != "time" &&
                    prefix != "json") {
                    features.usesNativeModules = true;
                }
            }
            for (auto& arg : call->args) scanExpr(arg.get());
            return;
        }

        if (auto binary = dynamic_cast<BinaryExpr*>(expr)) {
            scanExpr(binary->left.get());
            scanExpr(binary->right.get());
            return;
        }

        if (auto unary = dynamic_cast<UnaryExpr*>(expr)) {
            scanExpr(unary->right.get());
            return;
        }

        if (auto assign = dynamic_cast<AssignExpr*>(expr)) {
            scanExpr(assign->value.get());
            return;
        }

        if (auto index = dynamic_cast<IndexExpr*>(expr)) {
            scanExpr(index->target.get());
            scanExpr(index->index.get());
            return;
        }

        if (auto arrayExpr = dynamic_cast<ArrayExpr*>(expr)) {
            for (auto& item : arrayExpr->elements) scanExpr(item.get());
            return;
        }

        if (auto structInst = dynamic_cast<StructInstExpr*>(expr)) {
            for (auto& item : structInst->fields) scanExpr(item.second.get());
            return;
        }

        if (auto rng = dynamic_cast<RangeExpr*>(expr)) {
            scanExpr(rng->start.get());
            scanExpr(rng->end.get());
            return;
        }
    }

    void scanStmt(Stmt* stmt) {
        if (!stmt) return;

        if (auto importStmt = dynamic_cast<ImportStmt*>(stmt)) {
            if (importStmt->module == "rayquiro.app") features.usesApp = true;
            if (importStmt->module == "rayquiro.ui") features.usesUi = true;
            if (importStmt->module == "rayquiro.web") features.usesWeb = true;
            if (importStmt->module == "rayquiro.engine" || importStmt->module == "raytolfas.engine") features.usesEngine = true;
            if (importStmt->module == "rayquiro.fs") features.usesFs = true;
            if (importStmt->module == "rayquiro.env") features.usesEnv = true;
            if (importStmt->module == "rayquiro.process") features.usesProcess = true;
            if (importStmt->module == "rayquiro.os") features.usesOs = true;
            if (importStmt->module == "rayquiro.net") features.usesNet = true;
            if (importStmt->module == "rayquiro.http") features.usesHttp = true;
            if (importStmt->module == "rayquiro.db") features.usesDb = true;
            return;
        }

        if (auto fromImportStmt = dynamic_cast<FromImportStmt*>(stmt)) {
            if (fromImportStmt->module == "rayquiro.app") features.usesApp = true;
            if (fromImportStmt->module == "rayquiro.ui") features.usesUi = true;
            if (fromImportStmt->module == "rayquiro.web") features.usesWeb = true;
            if (fromImportStmt->module == "rayquiro.engine" || fromImportStmt->module == "raytolfas.engine") features.usesEngine = true;
            if (fromImportStmt->module == "rayquiro.fs") features.usesFs = true;
            if (fromImportStmt->module == "rayquiro.env") features.usesEnv = true;
            if (fromImportStmt->module == "rayquiro.process") features.usesProcess = true;
            if (fromImportStmt->module == "rayquiro.os") features.usesOs = true;
            if (fromImportStmt->module == "rayquiro.net") features.usesNet = true;
            if (fromImportStmt->module == "rayquiro.http") features.usesHttp = true;
            if (fromImportStmt->module == "rayquiro.db") features.usesDb = true;
            return;
        }

        if (auto varStmt = dynamic_cast<VarStmt*>(stmt)) {
            scanExpr(varStmt->initializer.get());
            return;
        }

        if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
            scanExpr(exprStmt->expr.get());
            return;
        }

        if (auto logStmt = dynamic_cast<LogStmt*>(stmt)) {
            scanExpr(logStmt->message.get());
            return;
        }

        if (auto blockStmt = dynamic_cast<BlockStmt*>(stmt)) {
            for (auto& child : blockStmt->statements) scanStmt(child.get());
            return;
        }

        if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
            scanExpr(whileStmt->condition.get());
            scanStmt(whileStmt->body.get());
            return;
        }

        if (auto ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            scanExpr(ifStmt->condition.get());
            scanStmt(ifStmt->thenBranch.get());
            scanStmt(ifStmt->elseBranch.get());
            return;
        }

        if (auto functionStmt = dynamic_cast<FunctionStmt*>(stmt)) {
            for (auto& child : functionStmt->body->statements) scanStmt(child.get());
            return;
        }

        if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) {
            for (const auto& v : returnStmt->values) scanExpr(v.get());
            return;
        }

        if (auto arrDestr = dynamic_cast<ArrayDestructureStmt*>(stmt)) {
            scanExpr(arrDestr->init.get());
            return;
        }

        if (auto objDestr = dynamic_cast<ObjectDestructureStmt*>(stmt)) {
            scanExpr(objDestr->init.get());
            return;
        }

        if (auto forIn = dynamic_cast<ForInStmt*>(stmt)) {
            scanExpr(forIn->iterable.get());
            scanStmt(forIn->body.get());
            return;
        }

        if (auto spawnStmt = dynamic_cast<SpawnStmt*>(stmt)) {
            scanExpr(spawnStmt->callExpr.get());
            return;
        }

        if (auto deferStmt = dynamic_cast<DeferStmt*>(stmt)) {
            scanStmt(deferStmt->statement.get());
            return;
        }

        if (auto testStmt = dynamic_cast<TestStmt*>(stmt)) {
            scanStmt(testStmt->body.get());
            return;
        }

        if (auto tc = dynamic_cast<TryCatchStmt*>(stmt)) {
            scanStmt(tc->tryBody.get());
            if (tc->catchBody) scanStmt(tc->catchBody.get());
            if (tc->finallyBody) scanStmt(tc->finallyBody.get());
            return;
        }

        if (auto matchStmt = dynamic_cast<MatchStmt*>(stmt)) {
            scanExpr(matchStmt->subject.get());
            for (auto& arm : matchStmt->arms) {
                if (arm.pattern) scanExpr(arm.pattern.get());
                if (arm.guard) scanExpr(arm.guard.get());
                scanStmt(arm.body.get());
            }
            return;
        }
    }

    std::string builtinName(const std::string& name) const {
        const std::string builtin = canonicalizeBuiltinName(name);
        if (builtin == "print" || builtin == "log.info") return "rq::print";
        if (builtin == "assert") return "rq::assert_val";
        if (builtin == "assert_eq") return "rq::assert_eq";
        if (builtin == "assert_ne") return "rq::assert_ne";
        if (builtin == "assert_true") return "rq::assert_true";
        if (builtin == "assert_false") return "rq::assert_false";
        if (builtin == "assert_null") return "rq::assert_null";
        if (builtin == "channel") return "rq::channel_create";
        if (builtin == "buffer.alloc" || builtin == "buffer_alloc") return "rq::buffer_alloc";
        if (builtin == "buffer.from_string" || builtin == "buffer_from_string") return "rq::buffer_from_string";
        if (builtin == "buffer.to_string" || builtin == "buffer_to_string") return "rq::buffer_to_string";
        if (builtin == "buffer.size" || builtin == "buffer_size" || builtin == "buffer.len") return "rq::buffer_size";
        if (builtin == "buffer.get_u8" || builtin == "buffer_get_u8") return "rq::buffer_get_u8";
        if (builtin == "buffer.set_u8" || builtin == "buffer_set_u8") return "rq::buffer_set_u8";
        if (builtin == "buffer.get_i32" || builtin == "buffer_get_i32") return "rq::buffer_get_i32";
        if (builtin == "buffer.set_i32" || builtin == "buffer_set_i32") return "rq::buffer_set_i32";
        if (builtin == "buffer.get_f64" || builtin == "buffer_get_f64") return "rq::buffer_get_f64";
        if (builtin == "buffer.set_f64" || builtin == "buffer_set_f64") return "rq::buffer_set_f64";
        if (builtin == "buffer.slice" || builtin == "buffer_slice") return "rq::buffer_slice";
        if (builtin == "struct.calcsize" || builtin == "struct_calcsize") return "rq::struct_calcsize";
        if (builtin == "struct.pack" || builtin == "struct_pack") return "rq::struct_pack";
        if (builtin == "struct.unpack" || builtin == "struct_unpack") return "rq::struct_unpack";
        if (builtin == "ffi.load" || builtin == "ffi_load") return "rq::ffi_load";
        if (builtin == "ffi.sym" || builtin == "ffi_sym") return "rq::ffi_sym";
        if (builtin == "ffi.call" || builtin == "ffi_call") return "rq::ffi_call";
        if (builtin == "ffi.close" || builtin == "ffi_close") return "rq::ffi_close";
        if (builtin == "ffi.callback" || builtin == "ffi_callback") return "rq::ffi_callback";
        if (builtin == "len") return "rq::len";
        if (builtin == "str") return "rq::str";
        if (builtin == "num") return "rq::num";
        if (builtin == "bool") return "rq::to_bool";
        if (builtin == "type") return "rq::type_name";
        if (builtin == "range") return "rq::range";
        if (builtin == "push") return "rq::push";
        if (builtin == "pop") return "rq::pop";
        if (builtin == "join") return "rq::join";
        if (builtin == "split") return "rq::split";
        if (builtin == "upper") return "rq::upper";
        if (builtin == "lower") return "rq::lower";
        if (builtin == "contains") return "rq::contains";
        if (builtin == "trim") return "rq::trim";
        if (builtin == "replace") return "rq::replace";
        if (builtin == "slice") return "rq::slice";
        if (builtin == "sqrt" || builtin == "math.sqrt") return "rq::sqrt_value";
        if (builtin == "pow" || builtin == "math.pow") return "rq::pow_value";
        if (builtin == "abs" || builtin == "math.abs") return "rq::abs_value";
        if (builtin == "sin" || builtin == "math.sin") return "rq::sin_value";
        if (builtin == "cos" || builtin == "math.cos") return "rq::cos_value";
        if (builtin == "tan" || builtin == "math.tan") return "rq::tan_value";
        if (builtin == "asin" || builtin == "math.asin") return "rq::asin_value";
        if (builtin == "acos" || builtin == "math.acos") return "rq::acos_value";
        if (builtin == "atan" || builtin == "math.atan") return "rq::atan_value";
        if (builtin == "exp" || builtin == "math.exp") return "rq::exp_value";
        if (builtin == "log" || builtin == "math.log") return "rq::log_value";
        if (builtin == "log2" || builtin == "math.log2") return "rq::log2_value";
        if (builtin == "log10" || builtin == "math.log10") return "rq::log10_value";
        if (builtin == "vec2") return "rq::vec2_value";
        if (builtin == "vec3") return "rq::vec3_value";
        if (builtin == "vec_add") return "rq::vec_add_value";
        if (builtin == "vec_sub") return "rq::vec_sub_value";
        if (builtin == "vec_mul") return "rq::vec_mul_value";
        if (builtin == "vec_dot") return "rq::vec_dot_value";
        if (builtin == "vec_len") return "rq::vec_len_value";
        if (builtin == "vec_normalize") return "rq::vec_normalize_value";
        if (builtin == "vec_dist") return "rq::vec_dist_value";
        if (builtin == "vec_lerp") return "rq::vec_lerp_value";
        if (builtin == "vec_cross") return "rq::vec_cross_value";
        if (builtin == "floor") return "rq::floor_value";
        if (builtin == "ceil") return "rq::ceil_value";
        if (builtin == "round") return "rq::round_value";
        if (builtin == "min") return "rq::min_value";
        if (builtin == "max") return "rq::max_value";
        if (builtin == "clamp") return "rq::clamp_value";
        if (builtin == "sleep") return "rq::sleep_ms";
        if (builtin == "clock.ms") return "rq::clock_ms";
        if (builtin == "time.now_ms") return "rq::clock_ms";
        if (builtin == "time.sleep") return "rq::sleep_ms";
        if (builtin == "time.unix_ms") return "rq::unix_ms";
        if (builtin == "json.stringify") return "rq::json_stringify";
        if (builtin == "json.parse") return "rq::json_parse";
        if (builtin == "random") return "rq::random_value";
        if (builtin == "random.int") return "rq::random_int";
        if (builtin == "web.page") return "rq::web_page";
        if (builtin == "web.begin") return "rq::web_begin";
        if (builtin == "web.route") return "rq::web_route";
        if (builtin == "web.head") return "rq::web_head";
        if (builtin == "web.public") return "rq::web_public";
        if (builtin == "web.live") return "rq::web_live";
        if (builtin == "web.serve") return "rq::web_live";
        if (builtin == "web.style") return "rq::web_style";
        if (builtin == "web.open") return "rq::web_open";
        if (builtin == "web.close") return "rq::web_close";
        if (builtin == "web.text") return "rq::web_text";
        if (builtin == "web.h1") return "rq::web_h1";
        if (builtin == "web.h2") return "rq::web_h2";
        if (builtin == "web.p") return "rq::web_p";
        if (builtin == "web.button") return "rq::web_button";
        if (builtin == "web.raw" || builtin == "web.html") return "rq::web_raw";
        if (builtin == "web.end") return "rq::web_end";
        if (builtin == "app.init") return "rq::app_init";
        if (builtin == "app.run") return "rq::app_run";
        if (builtin == "app.button") return "rq::app_button_value";
        if (builtin == "app.text") return "rq::app_text_value";
        if (builtin == "app.column") return "rq::app_column_value";
        if (builtin == "app.to_html" || builtin == "app.toHtml") return "rq::app_to_html_value";
        if (builtin == "app.dark") return "rq::ui_dark_value";
        if (builtin == "app.rgb") return "rq::ui_rgb_value";
        if (builtin == "app.blend") return "rq::ui_blend_value";
        if (builtin == "app.msg") return "rq::app_msg";
        if (builtin == "ui.dark") return "rq::ui_dark_value";
        if (builtin == "ui.rgb") return "rq::ui_rgb_value";
        if (builtin == "ui.blend") return "rq::ui_blend_value";
        if (builtin == "ui.init") return "rq::ui_init";
        if (builtin == "ui.style") return "rq::ui_style";
        if (builtin == "ui.hero") return "rq::ui_hero";
        if (builtin == "ui.status") return "rq::ui_status";
        if (builtin == "ui.info") return "rq::ui_info";
        if (builtin == "ui.text") return "rq::ui_text";
        if (builtin == "ui.action") return "rq::ui_action";
        if (builtin == "ui.button") return "rq::ui_action";
        if (builtin == "ui.run") return "rq::ui_run";
        if (builtin == "fs.exists") return "rq::fs_exists";
        if (builtin == "fs.mkdir") return "rq::fs_mkdir";
        if (builtin == "fs.copy") return "rq::fs_copy";
        if (builtin == "fs.copy_tree") return "rq::fs_copy_tree";
        if (builtin == "fs.remove") return "rq::fs_remove";
        if (builtin == "fs.read") return "rq::fs_read";
        if (builtin == "fs.write") return "rq::fs_write";
        if (builtin == "process.run") return "rq::process_run";
        if (builtin == "process.exe_dir") return "rq::process_exe_dir";
        if (builtin == "os.name") return "rq::os_name";
        if (builtin == "os.arch") return "rq::os_arch";
        if (builtin == "os.cwd" || builtin == "process.cwd") return "rq::process_cwd";
        if (builtin == "os.chdir") return "rq::os_chdir";
        if (builtin == "os.home") return "rq::os_home";
        if (builtin == "os.temp") return "rq::os_temp";
        if (builtin == "os.sep") return "rq::os_sep";
        if (builtin == "os.exists") return "rq::os_exists";
        if (builtin == "os.is_dir") return "rq::os_is_dir";
        if (builtin == "os.is_file") return "rq::os_is_file";
        if (builtin == "net.tcp_connect") return "rq::net_tcp_connect";
        if (builtin == "net.tcp_send") return "rq::net_tcp_send";
        if (builtin == "net.tcp_recv") return "rq::net_tcp_recv";
        if (builtin == "net.tcp_close") return "rq::net_tcp_close";
        if (builtin == "http.get") return "rq::http_get";
        if (builtin == "http.post") return "rq::http_post";
        if (builtin == "db.connect") return "rq::db_connect";
        if (builtin == "db.query") return "rq::db_query";
        if (builtin == "db.exec") return "rq::db_exec";
        if (builtin == "db.scalar") return "rq::db_scalar";
        if (builtin == "db.close") return "rq::db_close";
        if (builtin == "env.get") return "rq::env_get";
        if (builtin == "env.set") return "rq::env_set";
        if (builtin == "env.has") return "rq::env_has";
        if (builtin == "env.path_add") return "rq::env_path_add";
        if (builtin == "path.join") return "rq::path_join";
        if (builtin == "path.basename") return "rq::path_basename";
        if (builtin == "path.dirname") return "rq::path_dirname";
        if (builtin == "path.ext" || builtin == "path.extname") return "rq::path_ext";
        if (builtin == "path.exists") return "rq::path_exists";
        if (builtin == "path.is_abs") return "rq::path_is_abs";
        if (builtin == "hash.sha256" || builtin == "crypto.sha256") return "rq::sha256_value";
        if (builtin == "crypto.base64_encode") return "rq::base64_encode_value";
        if (builtin == "crypto.base64_decode") return "rq::base64_decode_value";
        if (builtin == "crypto.uuid") return "rq::uuid_value";
        if (builtin == "engine.init") return "rq::engine_init";
        if (builtin == "engine.shutdown") return "rq::engine_shutdown";
        if (builtin == "engine.should_close") return "rq::engine_should_close";
        if (builtin == "engine.begin") return "rq::engine_begin";
        if (builtin == "engine.end") return "rq::engine_end";
        if (builtin == "engine.clear") return "rq::engine_clear";
        if (builtin == "engine.set_camera") return "rq::engine_set_camera";
        if (builtin == "engine.target_fps") return "rq::engine_target_fps";
        if (builtin == "engine.frame_time") return "rq::engine_frame_time";
        if (builtin == "engine.backend") return "rq::engine_backend";
        if (builtin == "engine.backend_info") return "rq::engine_backend_info";
        if (builtin == "engine.backend_name") return "rq::engine_backend_name";
        if (builtin == "engine.vsync") return "rq::engine_vsync";
        if (builtin == "engine.msaa") return "rq::engine_msaa";
        if (builtin == "engine.exposure") return "rq::engine_exposure";
        if (builtin == "engine.vignette") return "rq::engine_vignette";
        if (builtin == "engine.film_grain") return "rq::engine_film_grain";
        if (builtin == "engine.saturation") return "rq::engine_saturation";
        if (builtin == "engine.contrast") return "rq::engine_contrast";
        if (builtin == "engine.bloom") return "rq::engine_bloom";
        if (builtin == "engine.fog") return "rq::engine_fog";
        if (builtin == "engine.volumetric") return "rq::engine_volumetric";
        if (builtin == "engine.postfx_info") return "rq::engine_postfx_info";
        if (builtin == "engine.scene_watch") return "rq::engine_scene_watch";
        if (builtin == "engine.scene_reload") return "rq::engine_scene_reload";
        if (builtin == "engine.stats") return "rq::engine_stats";
        if (builtin == "engine.render_stats") return "rq::engine_render_stats";
        if (builtin == "engine.camera_fov") return "rq::engine_camera_fov";
        if (builtin == "engine.camera_orbit") return "rq::engine_camera_orbit";
        if (builtin == "engine.key_down") return "rq::engine_key_down";
        if (builtin == "engine.key_pressed") return "rq::engine_key_pressed";
        if (builtin == "engine.mouse_down") return "rq::engine_mouse_down";
        if (builtin == "engine.mouse_pos") return "rq::engine_mouse_pos";
        if (builtin == "engine.window") return "rq::engine_window";
        if (builtin == "engine.camera") return "rq::engine_camera";
        if (builtin == "engine.frame_begin") return "rq::engine_frame_begin";
        if (builtin == "engine.frame_end") return "rq::engine_frame_end";
        if (builtin == "engine.scene") return "rq::engine_scene";
        if (builtin == "engine.scene_clear") return "rq::engine_scene_clear";
        if (builtin == "engine.scene_stats") return "rq::engine_scene_stats";
        if (builtin == "engine.export_scene") return "rq::engine_export_scene";
        if (builtin == "engine.scene_save") return "rq::engine_scene_save";
        if (builtin == "engine.scene_load") return "rq::engine_scene_load";
        if (builtin == "engine.capabilities" || builtin == "engine.backend_capabilities") return "rq::engine_capabilities_value";
        if (builtin == "engine.entity") return "rq::engine_entity_value";
        if (builtin == "engine.entity_get") return "rq::engine_entity_get_value";
        if (builtin == "engine.entity_transform") return "rq::engine_entity_transform_value";
        if (builtin == "engine.entity_tag") return "rq::engine_entity_tag_value";
        if (builtin == "engine.entity_has_tag") return "rq::engine_entity_has_tag_value";
        if (builtin == "engine.entities_with_tag") return "rq::engine_entities_with_tag_value";
        if (builtin == "engine.distance_3d") return "rq::engine_distance_3d_value";
        if (builtin == "engine.ray_sphere") return "rq::engine_ray_sphere_value";
        if (builtin == "engine.aabb_intersects") return "rq::engine_aabb_intersects_value";
        if (builtin == "engine.light_point") return "rq::engine_light_point_value";
        if (builtin == "engine.lights") return "rq::engine_lights_value";
        if (builtin == "engine.raycast") return "rq::engine_raycast_value";
        if (builtin == "engine.entity_exists") return "rq::engine_entity_exists";
        if (builtin == "engine.entity_remove") return "rq::engine_entity_remove";
        if (builtin == "engine.entity_set_position") return "rq::engine_entity_set_position";
        if (builtin == "engine.entity_get_position") return "rq::engine_entity_get_position";
        if (builtin == "engine.entity_set_size") return "rq::engine_entity_set_size";
        if (builtin == "engine.entity_set_scale") return "rq::engine_entity_set_scale";
        if (builtin == "engine.entity_set_radius") return "rq::engine_entity_set_radius";
        if (builtin == "engine.entity_set_color") return "rq::engine_entity_set_color";
        if (builtin == "engine.entity_set_visible") return "rq::engine_entity_set_visible";
        if (builtin == "engine.entity_mesh") return "rq::engine_entity_mesh";
        if (builtin == "engine.entity_texture") return "rq::engine_entity_texture";
        if (builtin == "engine.mesh") return "rq::engine_mesh";
        if (builtin == "engine.mesh_exists") return "rq::engine_mesh_exists";
        if (builtin == "engine.texture") return "rq::engine_texture";
        if (builtin == "engine.texture_exists") return "rq::engine_texture_exists";
        if (builtin == "engine.material") return "rq::engine_material";
        if (builtin == "engine.material_exists") return "rq::engine_material_exists";
        if (builtin == "engine.material_texture") return "rq::engine_material_texture";
        if (builtin == "engine.entity_material") return "rq::engine_entity_material";
        if (builtin == "engine.scene_draw") return "rq::engine_scene_draw";
        if (builtin == "engine.light_ambient") return "rq::engine_light_ambient";
        if (builtin == "engine.light_directional") return "rq::engine_light_directional";
        if (builtin == "engine.assets_root") return "rq::engine_assets_root";
        if (builtin == "engine.asset_path") return "rq::engine_asset_path";
        if (builtin == "engine.asset_exists") return "rq::engine_asset_exists";
        if (builtin == "engine.draw_grid") return "rq::engine_draw_grid";
        if (builtin == "engine.draw_cube") return "rq::engine_draw_cube";
        if (builtin == "engine.draw_plane") return "rq::engine_draw_plane";
        if (builtin == "engine.draw_sphere") return "rq::engine_draw_sphere";
        if (builtin == "engine.draw_text") return "rq::engine_draw_text";
        if (builtin == "engine.draw_fps") return "rq::engine_draw_fps";
        if (builtin == "engine.load_model") return "rq::engine_load_model";
        if (builtin == "engine.unload_model") return "rq::engine_unload_model";
        if (builtin == "engine.draw_model") return "rq::engine_draw_model";
        if (builtin == "engine.draw_model_ex") return "rq::engine_draw_model_ex";
        if (builtin == "engine.load_texture") return "rq::engine_load_texture";
        if (builtin == "engine.unload_texture") return "rq::engine_unload_texture";
        if (builtin == "engine.draw_texture") return "rq::engine_draw_texture";
        if (builtin == "engine.draw_texture_ex") return "rq::engine_draw_texture_ex";
        return "";
    }

    std::string genArgs(const std::vector<std::unique_ptr<Expr>>& args) {
        std::string value;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) value += ", ";
            value += genExpr(args[i].get());
        }
        return value;
    }

    std::string genValueVector(const std::vector<std::unique_ptr<Expr>>& args) {
        return "std::vector<rq::Value>{" + genArgs(args) + "}";
    }

    std::string genExpr(Expr* expr) {
        if (auto literal = dynamic_cast<LiteralExpr*>(expr)) {
            if (literal->kind == LiteralExpr::Kind::Number) {
                std::string number = literal->value;
                const bool hasDot = number.find('.') != std::string::npos;
                const bool hasExponent = number.find('e') != std::string::npos || number.find('E') != std::string::npos;
                if (!hasDot && !hasExponent) {
                    number += ".0";
                }
                return "rq::Value(" + number + ")";
            }
            if (literal->kind == LiteralExpr::Kind::String) {
                return "rq::Value(\"" + escapeString(literal->value) + "\")";
            }
            if (literal->kind == LiteralExpr::Kind::Bool) {
                return "rq::Value(" + literal->value + ")";
            }
            return "rq::Value()";
        }

        if (auto identifier = dynamic_cast<IdentifierExpr*>(expr)) {
            if (identifier->name == "pi" || identifier->name == "math.pi") {
                return "rq::Value(3.14159265358979323846)";
            }
            return mangleSymbol(identifier->name);
        }

        if (auto binary = dynamic_cast<BinaryExpr*>(expr)) {
            const std::string left = genExpr(binary->left.get());
            const std::string right = genExpr(binary->right.get());

            if (binary->op == "+") return "rq::add(" + left + ", " + right + ")";
            if (binary->op == "-") return "rq::sub(" + left + ", " + right + ")";
            if (binary->op == "*") return "rq::mul(" + left + ", " + right + ")";
            if (binary->op == "/") return "rq::div(" + left + ", " + right + ")";
            if (binary->op == "%") return "rq::mod(" + left + ", " + right + ")";
            if (binary->op == "==") return "rq::eq(" + left + ", " + right + ")";
            if (binary->op == "!=") return "rq::neq(" + left + ", " + right + ")";
            if (binary->op == "<") return "rq::lt(" + left + ", " + right + ")";
            if (binary->op == "<=") return "rq::lte(" + left + ", " + right + ")";
            if (binary->op == ">") return "rq::gt(" + left + ", " + right + ")";
            if (binary->op == ">=") return "rq::gte(" + left + ", " + right + ")";
            if (binary->op == "&&") return "rq::Value(rq::truthy(" + left + ") && rq::truthy(" + right + "))";
            if (binary->op == "||") return "rq::Value(rq::truthy(" + left + ") || rq::truthy(" + right + "))";
        }

        if (auto unary = dynamic_cast<UnaryExpr*>(expr)) {
            const std::string right = genExpr(unary->right.get());
            if (unary->op == "!") return "rq::Value(!rq::truthy(" + right + "))";
            if (unary->op == "-") return "rq::Value(-rq::to_number(" + right + "))";
        }

        if (auto call = dynamic_cast<CallExpr*>(expr)) {
            const std::string builtin = builtinName(call->callee);
            if (!builtin.empty()) {
                return builtin + "(" + genValueVector(call->args) + ")";
            }
            const std::string canonical = canonicalizeBuiltinName(call->callee);
            if (canonical.find('.') != std::string::npos) {
                const std::string prefix = canonical.substr(0, canonical.find('.'));
                const std::string method = canonical.substr(canonical.find('.') + 1);
                if (prefix != "app" && prefix != "ui" && prefix != "web" && prefix != "engine" &&
                    prefix != "fs" && prefix != "env" && prefix != "process" && prefix != "os" &&
                    prefix != "net" && prefix != "http" && prefix != "db" && prefix != "time" &&
                    prefix != "json" && prefix != "buffer" && prefix != "struct" && prefix != "ffi" &&
                    prefix != "hash" && prefix != "crypto" && prefix != "path" && prefix != "math" &&
                    prefix != "datetime") {
                    return "rq::method_call(" + mangleSymbol(prefix) + ", \"" + escapeString(method) + "\", " + genValueVector(call->args) + ")";
                }
                return "rq::native_call(\"" + escapeString(canonical) + "\", " + genValueVector(call->args) + ")";
            }
            auto fIt = userFunctionMap_.find(call->callee);
            if (fIt != userFunctionMap_.end()) {
                const auto* fn = fIt->second;
                if (!fn->params.empty() && fn->params.back().isVariadic) {
                    size_t regCount = fn->params.size() - 1;
                    std::string argsStr;
                    for (size_t i = 0; i < regCount; ++i) {
                        if (i > 0) argsStr += ", ";
                        if (i < call->args.size()) argsStr += genExpr(call->args[i].get());
                        else argsStr += "rq::Value()";
                    }
                    if (regCount > 0) argsStr += ", ";
                    std::string restVec = "{";
                    for (size_t i = regCount; i < call->args.size(); ++i) {
                        if (i > regCount) restVec += ", ";
                        restVec += genExpr(call->args[i].get());
                    }
                    restVec += "}";
                    argsStr += "rq::Value::from_array(" + restVec + ")";
                    return "::" + mangleSymbol(call->callee) + "(" + argsStr + ")";
                }
                return "::" + mangleSymbol(call->callee) + "(" + genArgs(call->args) + ")";
            }
            return mangleSymbol(call->callee) + "(" + genArgs(call->args) + ")";
        }

        if (auto dynCall = dynamic_cast<DynCallExpr*>(expr)) {
            return "(" + genExpr(dynCall->callee.get()) + ")(" + genArgs(dynCall->args) + ")";
        }

        if (auto lambda = dynamic_cast<LambdaExpr*>(expr)) {
            std::ostringstream ss;
            ss << "rq::Value(rq::Value::Fn([=](const std::vector<rq::Value>& _rq_args) -> rq::Value {\n";
            for (size_t i = 0; i < lambda->params.size(); ++i) {
                ss << "    rq::Value " << mangleSymbol(lambda->params[i].name)
                   << " = (_rq_args.size() > " << i << " ? _rq_args[" << i << "] : rq::Value());\n";
            }
            if (lambda->body) {
                for (auto& s : lambda->body->statements) {
                    genStmt(ss, s.get(), "    ");
                }
            }
            ss << "    return rq::Value();\n";
            ss << "}))";
            return ss.str();
        }

        if (auto nullCoalesce = dynamic_cast<NullCoalesceExpr*>(expr)) {
            return "rq::coalesce(" + genExpr(nullCoalesce->left.get()) + ", [&](){ return " + genExpr(nullCoalesce->right.get()) + "; })";
        }

        if (auto optChain = dynamic_cast<OptionalChainExpr*>(expr)) {
            std::string idx = optChain->index ? genExpr(optChain->index.get()) : ("rq::Value(\"" + escapeString(optChain->field) + "\")");
            return "rq::optional_chain(" + genExpr(optChain->object.get()) + ", " + idx + ")";
        }

        if (auto slice = dynamic_cast<SliceExpr*>(expr)) {
            std::string s = slice->start ? genExpr(slice->start.get()) : "rq::Value()";
            std::string e = slice->end ? genExpr(slice->end.get()) : "rq::Value()";
            return "rq::slice_expr(" + genExpr(slice->target.get()) + ", " + s + ", " + e + ")";
        }

        if (auto tmpl = dynamic_cast<TemplateLiteralExpr*>(expr)) {
            std::string res = "rq::Value(std::string(\"" + escapeString(tmpl->parts.empty() ? "" : tmpl->parts[0]) + "\")";
            for (size_t i = 0; i < tmpl->exprs.size(); ++i) {
                res += " + rq::to_string(" + genExpr(tmpl->exprs[i].get()) + ")";
                if (i + 1 < tmpl->parts.size()) {
                    res += " + std::string(\"" + escapeString(tmpl->parts[i + 1]) + "\")";
                }
            }
            res += ")";
            return res;
        }

        if (auto comp = dynamic_cast<CompoundAssignExpr*>(expr)) {
            std::string sym = mangleSymbol(comp->name);
            std::string val = genExpr(comp->value.get());
            if (comp->op == "+") return "(" + sym + " = rq::add(" + sym + ", " + val + "))";
            if (comp->op == "-") return "(" + sym + " = rq::sub(" + sym + ", " + val + "))";
            if (comp->op == "*") return "(" + sym + " = rq::mul(" + sym + ", " + val + "))";
            if (comp->op == "/") return "(" + sym + " = rq::div(" + sym + ", " + val + "))";
            if (comp->op == "%") return "(" + sym + " = rq::mod(" + sym + ", " + val + "))";
        }

        if (auto post = dynamic_cast<PostfixExpr*>(expr)) {
            std::string sym = mangleSymbol(post->name);
            if (post->op == "++") return "([&](){ rq::Value _old = " + sym + "; " + sym + " = rq::add(" + sym + ", rq::Value(1.0)); return _old; }())";
            if (post->op == "--") return "([&](){ rq::Value _old = " + sym + "; " + sym + " = rq::sub(" + sym + ", rq::Value(1.0)); return _old; }())";
        }

        if (auto arrayExpr = dynamic_cast<ArrayExpr*>(expr)) {
            return "rq::array(" + genValueVector(arrayExpr->elements) + ")";
        }

        if (auto objExpr = dynamic_cast<ObjectExpr*>(expr)) {
            std::string res = "rq::object({";
            for (size_t i = 0; i < objExpr->fields.size(); ++i) {
                if (i > 0) res += ", ";
                res += "{\"" + escapeString(objExpr->fields[i].first) + "\", " + genExpr(objExpr->fields[i].second.get()) + "}";
            }
            res += "})";
            return res;
        }

        if (auto structInst = dynamic_cast<StructInstExpr*>(expr)) {
            std::vector<std::pair<std::string, std::string>> fieldList;
            auto sIt = structDefaults_.find(structInst->typeName);
            if (sIt != structDefaults_.end()) {
                fieldList = sIt->second;
            }
            for (const auto& f : structInst->fields) {
                bool found = false;
                for (auto& ex : fieldList) {
                    if (ex.first == f.first) {
                        ex.second = genExpr(f.second.get());
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    fieldList.push_back({f.first, genExpr(f.second.get())});
                }
            }
            std::string res = "rq::object({";
            for (size_t i = 0; i < fieldList.size(); ++i) {
                if (i > 0) res += ", ";
                res += "{\"" + escapeString(fieldList[i].first) + "\", " + fieldList[i].second + "}";
            }
            res += "})";
            return res;
        }

        if (auto indexExpr = dynamic_cast<IndexExpr*>(expr)) {
            return "rq::index(" + genExpr(indexExpr->target.get()) + ", " + genExpr(indexExpr->index.get()) + ")";
        }

        if (auto setIdx = dynamic_cast<SetIndexExpr*>(expr)) {
            return "rq::set_index(" + genExpr(setIdx->object.get()) + ", " + genExpr(setIdx->index.get()) + ", " + genExpr(setIdx->value.get()) + ")";
        }

        if (auto rng = dynamic_cast<RangeExpr*>(expr)) {
            return "rq::range(std::vector<rq::Value>{" + genExpr(rng->start.get()) + ", " + genExpr(rng->end.get()) + "})";
        }

        if (auto assignExpr = dynamic_cast<AssignExpr*>(expr)) {
            return "(" + mangleSymbol(assignExpr->name) + " = " + genExpr(assignExpr->value.get()) + ")";
        }

        return "rq::Value()";
    }

    void genStmtBlockBody(std::ostream& outStream, Stmt* stmt, const std::string& indent) {
        if (auto block = dynamic_cast<BlockStmt*>(stmt)) {
            for (auto& child : block->statements) {
                genStmt(outStream, child.get(), indent);
            }
            return;
        }
        genStmt(outStream, stmt, indent);
    }

    void genStmt(std::ostream& outStream, Stmt* stmt, const std::string& indent, bool isTopLevel = false) {
        if (!stmt) return;

        if (auto varStmt = dynamic_cast<VarStmt*>(stmt)) {
            if (isTopLevel) {
                outStream << indent << mangleSymbol(varStmt->name) << " = " << genExpr(varStmt->initializer.get()) << ";\n";
            } else {
                outStream << indent << (varStmt->isLet ? "const " : "") << "rq::Value " << mangleSymbol(varStmt->name) << " = " << genExpr(varStmt->initializer.get()) << ";\n";
            }
            return;
        }

        if (auto arrDestr = dynamic_cast<ArrayDestructureStmt*>(stmt)) {
            std::string tmp = "_rq_tmp_" + std::to_string(tempVarCounter_++);
            outStream << indent << "rq::Value " << tmp << " = " << genExpr(arrDestr->init.get()) << ";\n";
            for (size_t i = 0; i < arrDestr->names.size(); ++i) {
                if (arrDestr->names[i].empty()) continue;
                if (isTopLevel) {
                    outStream << indent << mangleSymbol(arrDestr->names[i]) << " = rq::index(" << tmp << ", rq::Value(" << std::to_string(i) << ".0));\n";
                } else {
                    outStream << indent << "rq::Value " << mangleSymbol(arrDestr->names[i]) << " = rq::index(" << tmp << ", rq::Value(" << std::to_string(i) << ".0));\n";
                }
            }
            return;
        }

        if (auto objDestr = dynamic_cast<ObjectDestructureStmt*>(stmt)) {
            std::string tmp = "_rq_tmp_" + std::to_string(tempVarCounter_++);
            outStream << indent << "rq::Value " << tmp << " = " << genExpr(objDestr->init.get()) << ";\n";
            for (const auto& [key, local] : objDestr->bindings) {
                if (isTopLevel) {
                    outStream << indent << mangleSymbol(local) << " = rq::index(" << tmp << ", rq::Value(\"" << escapeString(key) << "\"));\n";
                } else {
                    outStream << indent << "rq::Value " << mangleSymbol(local) << " = rq::index(" << tmp << ", rq::Value(\"" << escapeString(key) << "\"));\n";
                }
            }
            return;
        }

        if (auto forIn = dynamic_cast<ForInStmt*>(stmt)) {
            std::string itName = "_rq_it_" + std::to_string(tempVarCounter_++);
            std::string itemVar = forIn->valVar.empty() ? forIn->keyVar : forIn->valVar;
            outStream << indent << "rq::Value " << itName << " = " << genExpr(forIn->iterable.get()) << ";\n";
            outStream << indent << "if (rq::is_array(" << itName << ")) {\n";
            outStream << indent << "    for (const auto& _item : *" << itName << ".as_array()) {\n";
            outStream << indent << "        rq::Value " << mangleSymbol(itemVar) << " = _item;\n";
            genStmtBlockBody(outStream, forIn->body.get(), indent + "        ");
            outStream << indent << "    }\n";
            outStream << indent << "}\n";
            return;
        }

        if (auto spawnStmt = dynamic_cast<SpawnStmt*>(stmt)) {
            outStream << indent << "std::thread([=]() mutable {\n";
            if (dynamic_cast<LambdaExpr*>(spawnStmt->callExpr.get())) {
                outStream << indent << "    (" << genExpr(spawnStmt->callExpr.get()) << ")();\n";
            } else if (dynamic_cast<CallExpr*>(spawnStmt->callExpr.get()) || dynamic_cast<DynCallExpr*>(spawnStmt->callExpr.get())) {
                outStream << indent << "    " << genExpr(spawnStmt->callExpr.get()) << ";\n";
            } else {
                std::string exprCode = genExpr(spawnStmt->callExpr.get());
                outStream << indent << "    auto _rq_spawn_target = " << exprCode << ";\n";
                outStream << indent << "    if (rq::is_function(_rq_spawn_target)) _rq_spawn_target();\n";
            }
            outStream << indent << "}).detach();\n";
            return;
        }

        if (auto deferStmt = dynamic_cast<DeferStmt*>(stmt)) {
            std::string defVar = "_rq_defer_" + std::to_string(tempVarCounter_++);
            outStream << indent << "rq::_RqDefer " << defVar << "([=]() mutable {\n";
            genStmt(outStream, deferStmt->statement.get(), indent + "    ");
            outStream << indent << "});\n";
            return;
        }

        if (auto testStmt = dynamic_cast<TestStmt*>(stmt)) {
            outStream << indent << "{\n";
            genStmtBlockBody(outStream, testStmt->body.get(), indent + "    ");
            outStream << indent << "}\n";
            return;
        }

        if (auto tc = dynamic_cast<TryCatchStmt*>(stmt)) {
            outStream << indent << "try {\n";
            genStmtBlockBody(outStream, tc->tryBody.get(), indent + "    ");
            outStream << indent << "} catch (const std::exception& _e) {\n";
            if (!tc->errorParam.empty()) {
                outStream << indent << "    rq::Value " << mangleSymbol(tc->errorParam) << " = rq::Value(_e.what());\n";
            }
            if (tc->catchBody) {
                genStmtBlockBody(outStream, tc->catchBody.get(), indent + "    ");
            }
            outStream << indent << "} catch (...) {\n";
            if (!tc->errorParam.empty()) {
                outStream << indent << "    rq::Value " << mangleSymbol(tc->errorParam) << " = rq::Value(\"error\");\n";
            }
            if (tc->catchBody) {
                genStmtBlockBody(outStream, tc->catchBody.get(), indent + "    ");
            }
            outStream << indent << "}\n";
            if (tc->finallyBody) {
                genStmtBlockBody(outStream, tc->finallyBody.get(), indent);
            }
            return;
        }

        if (auto th = dynamic_cast<ThrowStmt*>(stmt)) {
            outStream << indent << "throw std::runtime_error(rq::to_string(" << genExpr(th->value.get()) << "));\n";
            return;
        }

        if (auto matchStmt = dynamic_cast<MatchStmt*>(stmt)) {
            std::string subj = "_rq_subj_" + std::to_string(tempVarCounter_++);
            outStream << indent << "rq::Value " << subj << " = " << genExpr(matchStmt->subject.get()) << ";\n";
            bool first = true;
            for (const auto& arm : matchStmt->arms) {
                bool isWildcard = (!arm.pattern);
                if (auto id = dynamic_cast<IdentifierExpr*>(arm.pattern.get())) {
                    if (id->name == "_") isWildcard = true;
                }
                if (isWildcard) {
                    if (arm.guard) {
                        outStream << indent << (first ? "if" : "else if") << " (rq::truthy(" << genExpr(arm.guard.get()) << ")) {\n";
                        genStmt(outStream, arm.body.get(), indent + "    ");
                        outStream << indent << "}\n";
                        first = false;
                    } else {
                        outStream << indent << (first ? "" : "else ") << "{\n";
                        genStmt(outStream, arm.body.get(), indent + "    ");
                        outStream << indent << "}\n";
                        break;
                    }
                } else if (auto rng = dynamic_cast<RangeExpr*>(arm.pattern.get())) {
                    std::string cond = "(rq::truthy(rq::gte(" + subj + ", " + genExpr(rng->start.get()) + ")) && rq::truthy(rq::lte(" + subj + ", " + genExpr(rng->end.get()) + ")))";
                    if (arm.guard) {
                        cond += " && rq::truthy(" + genExpr(arm.guard.get()) + ")";
                    }
                    outStream << indent << (first ? "if" : "else if") << " (" << cond << ") {\n";
                    genStmt(outStream, arm.body.get(), indent + "    ");
                    outStream << indent << "}\n";
                    first = false;
                } else {
                    std::string cond = "(rq::truthy(rq::eq(" + subj + ", " + genExpr(arm.pattern.get()) + ")) || (rq::to_string(" + subj + ") == rq::to_string(" + genExpr(arm.pattern.get()) + ")))";
                    if (arm.guard) {
                        cond += " && rq::truthy(" + genExpr(arm.guard.get()) + ")";
                    }
                    outStream << indent << (first ? "if" : "else if") << " (" << cond << ") {\n";
                    genStmt(outStream, arm.body.get(), indent + "    ");
                    outStream << indent << "}\n";
                    first = false;
                }
            }
            return;
        }

        if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
            outStream << indent << genExpr(exprStmt->expr.get()) << ";\n";
            return;
        }

        if (auto logStmt = dynamic_cast<LogStmt*>(stmt)) {
            outStream << indent << "rq::print(std::vector<rq::Value>{" << genExpr(logStmt->message.get()) << "});\n";
            return;
        }

        if (auto blockStmt = dynamic_cast<BlockStmt*>(stmt)) {
            outStream << indent << "{\n";
            for (auto& child : blockStmt->statements) {
                genStmt(outStream, child.get(), indent + "    ");
            }
            outStream << indent << "}\n";
            return;
        }

        if (auto ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            outStream << indent << "if (rq::truthy(" << genExpr(ifStmt->condition.get()) << ")) {\n";
            genStmtBlockBody(outStream, ifStmt->thenBranch.get(), indent + "    ");
            outStream << indent << "}";
            if (ifStmt->elseBranch) {
                outStream << " else {\n";
                genStmtBlockBody(outStream, ifStmt->elseBranch.get(), indent + "    ");
                outStream << indent << "}";
            }
            outStream << "\n";
            return;
        }

        if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
            outStream << indent << "while (rq::truthy(" << genExpr(whileStmt->condition.get()) << ")) {\n";
            genStmtBlockBody(outStream, whileStmt->body.get(), indent + "    ");
            outStream << indent << "}\n";
            return;
        }

        if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) {
            if (returnStmt->values.empty()) {
                outStream << indent << "return rq::Value();\n";
            } else if (returnStmt->values.size() == 1) {
                outStream << indent << "return " << genExpr(returnStmt->values[0].get()) << ";\n";
            } else {
                outStream << indent << "return rq::Value::from_array({";
                for (size_t i = 0; i < returnStmt->values.size(); ++i) {
                    if (i > 0) outStream << ",";
                    outStream << genExpr(returnStmt->values[i].get());
                }
                outStream << "});\n";
            }
            return;
        }

        if (dynamic_cast<BreakStmt*>(stmt)) {
            outStream << indent << "break;\n";
            return;
        }

        if (dynamic_cast<ContinueStmt*>(stmt)) {
            outStream << indent << "continue;\n";
        }
    }

public:
    RuntimeFeatures generate(
        ProgramNode* root,
        const std::unordered_map<std::string, std::string>& namespaceAliases = {},
        const std::unordered_map<std::string, std::string>& symbolAliases = {},
        const std::string& outputPath = "output.cpp"
    ) {
        features = {};
        builtinNamespaceAliases = {
            {"app", "app"},
            {"ui", "ui"},
            {"web", "web"},
            {"engine", "engine"},
            {"json", "json"},
            {"time", "time"},
            {"fs", "fs"},
            {"env", "env"},
            {"process", "process"},
            {"os", "os"},
            {"net", "net"},
            {"http", "http"},
            {"db", "db"},
            {"buffer", "buffer"},
            {"struct", "struct"},
            {"ffi", "ffi"},
            {"hash", "hash"},
            {"crypto", "crypto"},
            {"path", "path"},
            {"math", "math"},
            {"datetime", "datetime"}
        };
        builtinSymbolAliases.clear();
        for (const auto& entry : namespaceAliases) {
            builtinNamespaceAliases[entry.first] = entry.second;
        }
        for (const auto& entry : symbolAliases) {
            builtinSymbolAliases[entry.first] = entry.second;
        }
        for (auto& statement : root->statements) {
            scanStmt(statement.get());
        }

        structDefaults_.clear();
        for (auto& statement : root->statements) {
            if (auto sStmt = dynamic_cast<StructStmt*>(statement.get())) {
                auto& defs = structDefaults_[sStmt->name];
                for (auto& f : sStmt->fields) {
                    defs.push_back({f.first, f.second ? genExpr(f.second.get()) : "rq::Value()"});
                }
            }
        }

        out.open(outputPath);
        out << "#include <algorithm>\n";
        out << "#include <chrono>\n";
        out << "#include <cmath>\n";
        out << "#include <cctype>\n";
        out << "#include <cstdlib>\n";
        out << "#include <cstdio>\n";
        out << "#include <filesystem>\n";
        out << "#include <fstream>\n";
        out << "#include <iostream>\n";
        out << "#include <memory>\n";
        out << "#include <optional>\n";
        out << "#include <random>\n";
        out << "#include <sstream>\n";
        out << "#include <string>\n";
        out << "#include <thread>\n";
        out << "#include <unordered_map>\n";
        out << "#include <variant>\n";
        out << "#include <vector>\n";
        out << "#include <cstring>\n";
        out << "#include <functional>\n";
        out << "#include <iomanip>\n";
        out << "#include <queue>\n";
        out << "#include <mutex>\n";
        out << "#include <condition_variable>\n";
        out << "#include <atomic>\n";
        if (!features.usesApp && !features.usesUi) {
            out << "#ifdef _WIN32\n";
            out << "#ifndef WIN32_LEAN_AND_MEAN\n";
            out << "#define WIN32_LEAN_AND_MEAN\n";
            out << "#endif\n";
            out << "#ifndef NOMINMAX\n";
            out << "#define NOMINMAX\n";
            out << "#endif\n";
            out << "#define Rectangle Win32Rectangle\n";
            out << "#define CloseWindow Win32CloseWindow\n";
            out << "#define ShowCursor Win32ShowCursor\n";
            out << "#define DrawText Win32DrawText\n";
            out << "#define DrawTextEx Win32DrawTextEx\n";
            out << "#define LoadImage Win32LoadImage\n";
            out << "#define PlaySound Win32PlaySound\n";
            out << "#endif\n";
        }
        out << "#ifdef _WIN32\n";
        if (features.usesWeb || features.usesNet || features.usesHttp) {
            out << "#include <winsock2.h>\n";
            out << "#include <ws2tcpip.h>\n";
        }
        if (!features.usesApp && !features.usesUi) {
            out << "#define Rectangle Win32Rectangle\n";
            out << "#define CloseWindow Win32CloseWindow\n";
            out << "#define ShowCursor Win32ShowCursor\n";
            out << "#define DrawText Win32DrawText\n";
            out << "#define DrawTextEx Win32DrawTextEx\n";
            out << "#define LoadImage Win32LoadImage\n";
            out << "#define PlaySound Win32PlaySound\n";
            out << "#include <windows.h>\n";
            out << "#undef Rectangle\n";
            out << "#undef CloseWindow\n";
            out << "#undef ShowCursor\n";
            out << "#undef DrawText\n";
            out << "#undef DrawTextEx\n";
            out << "#undef LoadImage\n";
            out << "#undef PlaySound\n";
        }
        out << "#else\n";
        out << "#include <dlfcn.h>\n";
        if (features.usesWeb || features.usesNet || features.usesHttp) {
            out << "#include <arpa/inet.h>\n";
            out << "#include <netinet/in.h>\n";
            out << "#include <sys/socket.h>\n";
            out << "#include <unistd.h>\n";
            out << "using SOCKET = int;\n";
            out << "static constexpr SOCKET INVALID_SOCKET = -1;\n";
            out << "static constexpr int SOCKET_ERROR = -1;\n";
            out << "inline int closesocket(SOCKET socket) { return ::close(socket); }\n";
        }
        out << "#endif\n";
        if (features.usesEnv || features.usesProcess) {
            out << "#ifdef _WIN32\n";
            if (features.usesProcess) out << "#include <libloaderapi.h>\n";
            if (features.usesEnv) out << "#include <winreg.h>\n";
            out << "#endif\n";
        }
        if (features.usesApp) out << "#include \"Framework.h\"\n";
        if (features.usesUi) out << "#include \"ModernUI.h\"\n";
        if (features.usesEngine) out << "#include \"rte_api.h\"\n";
        out << "\n";

        RuntimeEmitter::emit(out, features);

        userFunctionMap_.clear();
        for (auto& statement : root->statements) {
            if (auto functionStmt = dynamic_cast<FunctionStmt*>(statement.get())) {
                userFunctionMap_[functionStmt->name] = functionStmt;
                out << "rq::Value " << mangleSymbol(functionStmt->name) << "(";
                for (size_t i = 0; i < functionStmt->params.size(); ++i) {
                    if (i) out << ", ";
                    out << "rq::Value " << mangleSymbol(functionStmt->params[i].name);
                    if (functionStmt->params[i].defaultValue) {
                        out << " = " << genExpr(functionStmt->params[i].defaultValue.get());
                    }
                }
                out << ");\n";
            }
        }
        out << "\n";

        for (auto& statement : root->statements) {
            if (auto varStmt = dynamic_cast<VarStmt*>(statement.get())) {
                out << "rq::Value " << mangleSymbol(varStmt->name) << ";\n";
            } else if (auto arrDestr = dynamic_cast<ArrayDestructureStmt*>(statement.get())) {
                for (const auto& name : arrDestr->names) {
                    if (!name.empty()) out << "rq::Value " << mangleSymbol(name) << ";\n";
                }
            } else if (auto objDestr = dynamic_cast<ObjectDestructureStmt*>(statement.get())) {
                for (const auto& [k, v] : objDestr->bindings) {
                    out << "rq::Value " << mangleSymbol(v) << ";\n";
                }
            }
        }
        out << "\n";

        for (auto& statement : root->statements) {
            if (auto functionStmt = dynamic_cast<FunctionStmt*>(statement.get())) {
                out << "rq::Value " << mangleSymbol(functionStmt->name) << "(";
                for (size_t i = 0; i < functionStmt->params.size(); ++i) {
                    if (i) out << ", ";
                    out << "rq::Value " << mangleSymbol(functionStmt->params[i].name);
                }
                out << ") {\n";
                for (auto& bodyStmt : functionStmt->body->statements) {
                    genStmt(out, bodyStmt.get(), "    ");
                }
                out << "    return rq::Value();\n";
                out << "}\n\n";
            }
        }

        out << "int main() {\n";
        for (auto& statement : root->statements) {
            if (dynamic_cast<FunctionStmt*>(statement.get())) continue;
            if (dynamic_cast<ImportStmt*>(statement.get())) continue;
            if (dynamic_cast<FromImportStmt*>(statement.get())) continue;
            if (dynamic_cast<StructStmt*>(statement.get())) continue;
            if (dynamic_cast<EnumStmt*>(statement.get())) continue;
            if (dynamic_cast<InterfaceStmt*>(statement.get())) continue;
            genStmt(out, statement.get(), "    ", true);
        }
        out << "    return 0;\n";
        out << "}\n";
        out.close();

        return features;
    }
};

