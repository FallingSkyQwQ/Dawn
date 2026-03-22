#pragma once

#include "dawn/core/model/content_lock.h"
#include "dawn/core/model/instance_manifest.h"
#include "dawn/infra/json/simple_json.h"

#include <filesystem>
#include <string>

namespace dawn::core {

infra::json::Value instance_manifest_to_json(const InstanceManifest& manifest);
bool instance_manifest_from_json(const infra::json::Value& json, InstanceManifest* manifest, std::string* error = nullptr);
std::string instance_manifest_to_text(const InstanceManifest& manifest);
bool save_instance_manifest(const std::filesystem::path& path, const InstanceManifest& manifest, std::string* error = nullptr);
bool load_instance_manifest(const std::filesystem::path& path, InstanceManifest* manifest, std::string* error = nullptr);

infra::json::Value content_lock_to_json(const ContentLock& lock);
bool content_lock_from_json(const infra::json::Value& json, ContentLock* lock, std::string* error = nullptr);
std::string content_lock_to_text(const ContentLock& lock);
bool save_content_lock(const std::filesystem::path& path, const ContentLock& lock, std::string* error = nullptr);
bool load_content_lock(const std::filesystem::path& path, ContentLock* lock, std::string* error = nullptr);

} // namespace dawn::core
