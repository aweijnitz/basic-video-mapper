#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "projection/core/Cue.h"
#include "projection/core/Project.h"
#include "projection/core/Scene.h"
#include "repo/CueRepository.h"
#include "repo/ProjectRepository.h"
#include "repo/SceneRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <vector>

using projection::core::Cue;
using projection::core::Project;
using projection::core::ProjectSettings;
using projection::core::Scene;
using projection::core::makeCueId;
using projection::core::makeProjectId;
using projection::core::makeSceneId;
using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;
using projection::server::repo::CueRepository;
using projection::server::repo::ProjectRepository;
using projection::server::repo::SceneRepository;

namespace {
std::filesystem::path tempDb(const std::string& name) { return std::filesystem::temp_directory_path() / name; }
}  // namespace

TEST_CASE("ProjectRepository persists and retrieves ordered cues", "[repo][project]") {
    SqliteConnection connection;
    auto dbPath = tempDb("project_repo.sqlite");
    std::filesystem::remove(dbPath);
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);

    SceneRepository sceneRepo(connection);
    CueRepository cueRepo(connection);
    ProjectRepository projectRepo(connection);

    Scene scene{makeSceneId("1"), "Scene", "desc", {}};
    sceneRepo.createScene(scene);

    Cue cueA{makeCueId("cue-A"), "A", scene.getId()};
    Cue cueB{makeCueId("cue-B"), "B", scene.getId()};
    cueRepo.createCue(cueA);
    cueRepo.createCue(cueB);

    ProjectSettings settings;
    settings.midiChannels = {1};
    settings.controllers["fader1"] = "master";
    Project project{makeProjectId("proj-1"), "Show", "Demo", {cueA.getId(), cueB.getId()}, settings};
    projectRepo.createProject(project);

    auto projects = projectRepo.listProjects();
    REQUIRE(projects.size() == 1);
    REQUIRE(projects.front().getCueOrder().size() == 2);
    REQUIRE(projects.front().getCueOrder()[0] == cueA.getId());
    REQUIRE(projects.front().getSettings().controllers.at("fader1") == "master");

    Project updated = project;
    updated.setDescription("Updated");
    updated.getCueOrder() = {cueB.getId()};
    auto updatedSettings = updated.getSettings();
    updatedSettings.midiChannels = {2, 3};
    updated.setSettings(updatedSettings);
    projectRepo.updateProject(updated);

    auto fetched = projectRepo.findProjectById(project.getId());
    REQUIRE(fetched.has_value());
    REQUIRE(fetched->getDescription() == "Updated");
    REQUIRE(fetched->getCueOrder().size() == 1);
    REQUIRE(fetched->getCueOrder()[0] == cueB.getId());
    std::vector<int> updatedChannels{2, 3};
    REQUIRE(fetched->getSettings().midiChannels == updatedChannels);
}
