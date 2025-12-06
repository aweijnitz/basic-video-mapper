#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"
#include "repo/SceneRepository.h"
#include "repo/SurfaceRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <cmath>
#include <vector>

using projection::core::BlendMode;
using projection::core::FeedId;
using projection::core::Scene;
using projection::core::SceneId;
using projection::core::Surface;
using projection::core::SurfaceId;
using projection::core::Vec2;
using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;
using projection::server::repo::SceneRepository;
using projection::server::repo::SurfaceRepository;

namespace {
std::filesystem::path tempDbPath(const std::string& name) {
    auto dir = std::filesystem::temp_directory_path();
    return dir / name;
}
}

TEST_CASE("SurfaceRepository can persist and list surfaces for a scene", "[repo][surface]") {
    SqliteConnection connection;
    auto dbPath = tempDbPath("surface_repo.sqlite");
    std::filesystem::remove(dbPath);
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);

    SceneRepository sceneRepo(connection);
    SurfaceRepository surfaceRepo(connection);

    Scene scene(SceneId{"1"}, "Scene", "desc", {});
    sceneRepo.createScene(scene);

    std::vector<Vec2> quad{{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
    Surface surfaceA(SurfaceId{"sA"}, "A", quad, FeedId{"f1"}, 0.5f, 0.8f, BlendMode::Additive, 2);
    Surface surfaceB(SurfaceId{"sB"}, "B", quad, FeedId{"f2"}, 1.0f, 1.0f, BlendMode::Normal, 1);

    surfaceRepo.createSurface(surfaceA, scene.getId());
    surfaceRepo.createSurface(surfaceB, scene.getId());

    auto surfaces = surfaceRepo.listSurfacesForScene(scene.getId());
    REQUIRE(surfaces.size() == 2);

    auto first = surfaces[0];
    auto second = surfaces[1];

    REQUIRE(first.getId().value == "sB");
    REQUIRE(first.getZOrder() == 1);
    REQUIRE(first.getBlendMode() == BlendMode::Normal);
    REQUIRE(first.getVertices().size() == 4);

    REQUIRE(second.getId().value == "sA");
    REQUIRE(std::fabs(second.getOpacity() - 0.5f) < 0.0001f);
    REQUIRE(std::fabs(second.getBrightness() - 0.8f) < 0.0001f);
    REQUIRE(second.getBlendMode() == BlendMode::Additive);
}
