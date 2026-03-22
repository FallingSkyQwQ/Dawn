#include "dawn/core/serialization/manifest_codec.h"

#include "dawn/infra/fs/file_system.h"

#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;

Value make_string(const std::string& value) {
    return Value(value);
}

Value make_bool(bool value) {
    return Value(value);
}

Value make_string_array(const std::vector<std::string>& values) {
    Value::Array array;
    array.reserve(values.size());
    for (const auto& value : values) {
        array.emplace_back(value);
    }
    return Value(std::move(array));
}

std::vector<std::string> read_string_array(const Value* value) {
    std::vector<std::string> result;
    if (!value || !value->is_array()) {
        return result;
    }
    for (const auto& entry : value->as_array()) {
        if (entry.is_string()) {
            result.push_back(entry.as_string());
        }
    }
    return result;
}

bool require_string(const Value::Object& object, const std::string& key, std::string* out, std::string* error) {
    const auto* value = dawn::infra::json::find(object, key);
    if (!value || !value->is_string()) {
        if (error) {
            *error = "missing or invalid string field: " + key;
        }
        return false;
    }
    if (out) {
        *out = value->as_string();
    }
    return true;
}

bool require_bool(const Value::Object& object, const std::string& key, bool* out, std::string* error, bool fallback = false) {
    const auto* value = dawn::infra::json::find(object, key);
    if (!value) {
        if (out) {
            *out = fallback;
        }
        return true;
    }
    if (!value->is_bool()) {
        if (error) {
            *error = "missing or invalid bool field: " + key;
        }
        return false;
    }
    if (out) {
        *out = value->as_bool();
    }
    return true;
}

Value serialize_loader_type(LoaderType loader) {
    return Value(std::string(to_string(loader)));
}

LoaderType parse_loader_type(const Value* value) {
    if (!value || !value->is_string()) {
        return LoaderType::None;
    }
    return loader_type_from_string(value->as_string());
}

Value serialize_instance(const InstanceManifest& manifest) {
    Value::Object object;
    object.emplace("id", make_string(manifest.id));
    object.emplace("name", make_string(manifest.name));
    object.emplace("icon", make_string(manifest.icon));
    object.emplace("mcVersion", make_string(manifest.mcVersion));
    object.emplace("loaderType", serialize_loader_type(manifest.loaderType));
    object.emplace("loaderVersion", make_string(manifest.loaderVersion));
    object.emplace("optifineVersion", make_string(manifest.optifineVersion));
    object.emplace("javaProfileId", make_string(manifest.javaProfileId));
    object.emplace("memoryProfile", make_string(manifest.memoryProfile));
    object.emplace("gameDir", make_string(manifest.gameDir));
    object.emplace("createdAt", make_string(manifest.createdAt));
    object.emplace("lastPlayedAt", make_string(manifest.lastPlayedAt));
    object.emplace("tags", make_string_array(manifest.tags));
    object.emplace("notes", make_string(manifest.notes));
    object.emplace("themeColor", make_string(manifest.themeColor));
    return Value(std::move(object));
}

Value serialize_lock(const ContentLock& lock) {
    Value::Object object;
    object.emplace("provider", make_string(lock.provider));
    object.emplace("projectId", make_string(lock.projectId));
    object.emplace("versionId", make_string(lock.versionId));
    object.emplace("fileHash", make_string(lock.fileHash));
    object.emplace("installedPath", make_string(lock.installedPath.generic_string()));
    object.emplace("enabled", make_bool(lock.enabled));
    object.emplace("dependencies", make_string_array(lock.dependencies));
    return Value(std::move(object));
}

} // namespace

infra::json::Value instance_manifest_to_json(const InstanceManifest& manifest) {
    return serialize_instance(manifest);
}

bool instance_manifest_from_json(const infra::json::Value& json, InstanceManifest* manifest, std::string* error) {
    if (!manifest || !json.is_object()) {
        if (error) {
            *error = "instance manifest json must be an object";
        }
        return false;
    }

    const auto& object = json.as_object();
    if (!require_string(object, "id", &manifest->id, error) ||
        !require_string(object, "name", &manifest->name, error) ||
        !require_string(object, "icon", &manifest->icon, error) ||
        !require_string(object, "mcVersion", &manifest->mcVersion, error) ||
        !require_string(object, "loaderVersion", &manifest->loaderVersion, error) ||
        !require_string(object, "optifineVersion", &manifest->optifineVersion, error) ||
        !require_string(object, "javaProfileId", &manifest->javaProfileId, error) ||
        !require_string(object, "memoryProfile", &manifest->memoryProfile, error) ||
        !require_string(object, "gameDir", &manifest->gameDir, error) ||
        !require_string(object, "createdAt", &manifest->createdAt, error) ||
        !require_string(object, "lastPlayedAt", &manifest->lastPlayedAt, error) ||
        !require_string(object, "notes", &manifest->notes, error) ||
        !require_string(object, "themeColor", &manifest->themeColor, error)) {
        return false;
    }

    manifest->loaderType = parse_loader_type(dawn::infra::json::find(object, "loaderType"));
    manifest->tags = read_string_array(dawn::infra::json::find(object, "tags"));
    return true;
}

std::string instance_manifest_to_text(const InstanceManifest& manifest) {
    return dawn::infra::json::stringify(instance_manifest_to_json(manifest), 2);
}

bool save_instance_manifest(const std::filesystem::path& path, const InstanceManifest& manifest, std::string* error) {
    return dawn::infra::fs::write_text_file(path, instance_manifest_to_text(manifest), error);
}

bool load_instance_manifest(const std::filesystem::path& path, InstanceManifest* manifest, std::string* error) {
    std::string text;
    if (!dawn::infra::fs::read_text_file(path, &text, error)) {
        return false;
    }

    const auto parsed = dawn::infra::json::parse(text);
    if (!parsed.ok) {
        if (error) {
            *error = parsed.error.message;
        }
        return false;
    }
    return instance_manifest_from_json(parsed.value, manifest, error);
}

infra::json::Value content_lock_to_json(const ContentLock& lock) {
    return serialize_lock(lock);
}

bool content_lock_from_json(const infra::json::Value& json, ContentLock* lock, std::string* error) {
    if (!lock || !json.is_object()) {
        if (error) {
            *error = "content lock json must be an object";
        }
        return false;
    }

    const auto& object = json.as_object();
    if (!require_string(object, "provider", &lock->provider, error) ||
        !require_string(object, "projectId", &lock->projectId, error) ||
        !require_string(object, "versionId", &lock->versionId, error) ||
        !require_string(object, "fileHash", &lock->fileHash, error)) {
        return false;
    }

    const auto* installed_path = dawn::infra::json::find(object, "installedPath");
    if (!installed_path || !installed_path->is_string()) {
        if (error) {
            *error = "missing or invalid string field: installedPath";
        }
        return false;
    }
    lock->installedPath = std::filesystem::path(installed_path->as_string());

    if (!require_bool(object, "enabled", &lock->enabled, error, true)) {
        return false;
    }
    lock->dependencies = read_string_array(dawn::infra::json::find(object, "dependencies"));
    return true;
}

std::string content_lock_to_text(const ContentLock& lock) {
    return dawn::infra::json::stringify(content_lock_to_json(lock), 2);
}

bool save_content_lock(const std::filesystem::path& path, const ContentLock& lock, std::string* error) {
    return dawn::infra::fs::write_text_file(path, content_lock_to_text(lock), error);
}

bool load_content_lock(const std::filesystem::path& path, ContentLock* lock, std::string* error) {
    std::string text;
    if (!dawn::infra::fs::read_text_file(path, &text, error)) {
        return false;
    }

    const auto parsed = dawn::infra::json::parse(text);
    if (!parsed.ok) {
        if (error) {
            *error = parsed.error.message;
        }
        return false;
    }
    return content_lock_from_json(parsed.value, lock, error);
}

} // namespace dawn::core
