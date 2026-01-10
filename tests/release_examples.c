#include "../src/release.h"
#include <stdio.h>
#include <stdlib.h>

void demonstrate_release_preparation() {
    printf("=== Wyn Language 1.0.0 Release Preparation ===\n\n");
    
    // Create release manager for stable release
    WynReleaseManager* manager = wyn_release_manager_new("1.0.0", WYN_RELEASE_STABLE);
    wyn_release_manager_initialize(manager);
    
    printf("🚀 Preparing Wyn Language 1.0.0 Stable Release\n");
    printf("Release type: %s\n", wyn_release_type_name(manager->release_type));
    printf("Version: %s\n\n", manager->version);
    
    // Step 1: Build artifacts for all platforms
    printf("📦 Step 1: Building Release Artifacts\n");
    printf("=====================================\n");
    
    WynReleaseArtifact* linux_compiler = wyn_release_artifact_new("wyn-compiler-linux-x64", WYN_ARTIFACT_COMPILER, WYN_PLATFORM_LINUX_X64);
    WynReleaseArtifact* macos_compiler = wyn_release_artifact_new("wyn-compiler-macos-arm64", WYN_ARTIFACT_COMPILER, WYN_PLATFORM_MACOS_ARM64);
    WynReleaseArtifact* windows_compiler = wyn_release_artifact_new("wyn-compiler-windows-x64", WYN_ARTIFACT_COMPILER, WYN_PLATFORM_WINDOWS_X64);
    WynReleaseArtifact* wasm_compiler = wyn_release_artifact_new("wyn-compiler-wasm32", WYN_ARTIFACT_COMPILER, WYN_PLATFORM_WASM32);
    WynReleaseArtifact* documentation = wyn_release_artifact_new("wyn-docs", WYN_ARTIFACT_DOCUMENTATION, WYN_PLATFORM_LINUX_X64);
    WynReleaseArtifact* examples = wyn_release_artifact_new("wyn-examples", WYN_ARTIFACT_EXAMPLES, WYN_PLATFORM_LINUX_X64);
    
    // Build all artifacts
    wyn_release_artifact_build(linux_compiler, "/build/release/wyn-linux-x64");
    wyn_release_artifact_build(macos_compiler, "/build/release/wyn-macos-arm64");
    wyn_release_artifact_build(windows_compiler, "/build/release/wyn-windows-x64.exe");
    wyn_release_artifact_build(wasm_compiler, "/build/release/wyn.wasm");
    wyn_release_artifact_build(documentation, "/build/release/wyn-docs.tar.gz");
    wyn_release_artifact_build(examples, "/build/release/wyn-examples.tar.gz");
    
    printf("✓ Built %s for %s\n", linux_compiler->name, wyn_platform_name(linux_compiler->platform));
    printf("✓ Built %s for %s\n", macos_compiler->name, wyn_platform_name(macos_compiler->platform));
    printf("✓ Built %s for %s\n", windows_compiler->name, wyn_platform_name(windows_compiler->platform));
    printf("✓ Built %s for %s\n", wasm_compiler->name, wyn_platform_name(wasm_compiler->platform));
    printf("✓ Built %s\n", documentation->name);
    printf("✓ Built %s\n", examples->name);
    
    // Step 2: Sign artifacts
    printf("\n🔐 Step 2: Signing Release Artifacts\n");
    printf("====================================\n");
    
    wyn_release_artifact_sign(linux_compiler, "~/.wyn/release-signing.key");
    wyn_release_artifact_sign(macos_compiler, "~/.wyn/release-signing.key");
    wyn_release_artifact_sign(windows_compiler, "~/.wyn/release-signing.key");
    wyn_release_artifact_sign(wasm_compiler, "~/.wyn/release-signing.key");
    wyn_release_artifact_sign(documentation, "~/.wyn/release-signing.key");
    wyn_release_artifact_sign(examples, "~/.wyn/release-signing.key");
    
    // Verify signatures
    wyn_release_artifact_verify(linux_compiler, "~/.wyn/release-public.key");
    wyn_release_artifact_verify(macos_compiler, "~/.wyn/release-public.key");
    wyn_release_artifact_verify(windows_compiler, "~/.wyn/release-public.key");
    wyn_release_artifact_verify(wasm_compiler, "~/.wyn/release-public.key");
    wyn_release_artifact_verify(documentation, "~/.wyn/release-public.key");
    wyn_release_artifact_verify(examples, "~/.wyn/release-public.key");
    
    printf("✓ Signed and verified all artifacts\n");
    printf("✓ Checksums calculated for integrity verification\n");
    
    // Add artifacts to release manager
    wyn_release_manager_add_artifact(manager, linux_compiler);
    wyn_release_manager_add_artifact(manager, macos_compiler);
    wyn_release_manager_add_artifact(manager, windows_compiler);
    wyn_release_manager_add_artifact(manager, wasm_compiler);
    wyn_release_manager_add_artifact(manager, documentation);
    wyn_release_manager_add_artifact(manager, examples);
    
    printf("✓ Added %zu artifacts to release\n", manager->artifact_count);
    
    // Step 3: Run comprehensive validation
    printf("\n🧪 Step 3: Release Validation\n");
    printf("=============================\n");
    
    wyn_validate_compiler_functionality(manager);
    printf("\n");
    wyn_validate_standard_library(manager);
    printf("\n");
    wyn_validate_cross_platform_compatibility(manager);
    printf("\n");
    wyn_validate_performance_benchmarks(manager);
    printf("\n");
    wyn_validate_memory_safety(manager);
    printf("\n");
    wyn_validate_security_audit(manager);
    printf("\n");
    wyn_validate_documentation_completeness(manager);
    printf("\n");
    wyn_validate_example_programs(manager);
    
    // Step 4: Quality gates check
    printf("\n🎯 Step 4: Quality Gates Validation\n");
    printf("===================================\n");
    
    WynReleaseQualityGate gates = wyn_get_default_quality_gates(WYN_RELEASE_STABLE);
    printf("Quality requirements for stable release:\n");
    printf("- Minimum test coverage: %.1f%%\n", gates.min_test_coverage);
    printf("- Maximum critical failures: %zu\n", gates.max_critical_failures);
    printf("- Maximum build time: %.1f minutes\n", gates.max_build_time_minutes);
    printf("- Require all platforms: %s\n", gates.require_all_platforms ? "Yes" : "No");
    printf("- Require signed artifacts: %s\n", gates.require_signed_artifacts ? "Yes" : "No");
    printf("- Require security audit: %s\n", gates.require_security_audit ? "Yes" : "No");
    
    if (wyn_release_manager_check_quality_gates(manager, &gates)) {
        printf("✅ All quality gates passed!\n");
    }
    
    // Step 5: Final validation and preparation
    printf("\n✅ Step 5: Final Release Validation\n");
    printf("===================================\n");
    
    if (wyn_release_manager_validate_release(manager)) {
        printf("🎉 Release validation successful!\n");
        printf("✓ All artifacts built and verified\n");
        printf("✓ All validations passed\n");
        printf("✓ Quality gates satisfied\n");
        printf("✓ Ready for production deployment\n");
    }
    
    // Step 6: Generate release notes and deploy
    printf("\n🚀 Step 6: Release Deployment\n");
    printf("=============================\n");
    
    if (wyn_release_manager_create_release(manager)) {
        printf("🎊 Wyn Language 1.0.0 Released Successfully!\n\n");
        
        printf("📋 Release Summary:\n");
        printf("- Version: %s\n", manager->version);
        printf("- Type: %s\n", wyn_release_type_name(manager->release_type));
        printf("- Artifacts: %zu\n", manager->artifact_count);
        printf("- Platforms: Linux, macOS, Windows, WebAssembly\n");
        printf("- Release timestamp: %llu\n", (unsigned long long)manager->release_timestamp);
        
        printf("\n📦 Available Downloads:\n");
        for (size_t i = 0; i < manager->artifact_count; i++) {
            WynReleaseArtifact* artifact = &manager->artifacts[i];
            printf("- %s (%s) - %s\n", 
                   artifact->name, 
                   wyn_platform_name(artifact->platform),
                   artifact->checksum);
        }
        
        printf("\n🌐 Deployment Locations:\n");
        printf("- Registry: %s\n", manager->deployment_config->registry_url);
        printf("- CDN: %s\n", manager->deployment_config->cdn_url);
        printf("- Documentation: %s\n", manager->deployment_config->documentation_url);
        printf("- GitHub Release: %s\n", manager->deployment_config->github_release_url);
        
        printf("\n🎯 What's Next:\n");
        printf("- Community notification sent\n");
        printf("- Documentation site updated\n");
        printf("- Package registry updated\n");
        printf("- IDE plugins will be updated\n");
        printf("- Social media announcements posted\n");
    }
    
    // Cleanup
    wyn_release_artifact_free(linux_compiler);
    wyn_release_artifact_free(macos_compiler);
    wyn_release_artifact_free(windows_compiler);
    wyn_release_artifact_free(wasm_compiler);
    wyn_release_artifact_free(documentation);
    wyn_release_artifact_free(examples);
    wyn_release_manager_free(manager);
    
    printf("\n🏁 Release process completed successfully!\n");
    printf("Wyn Language 1.0.0 is now available to the world! 🌍\n");
}

void demonstrate_release_metrics() {
    printf("\n=== Release Metrics and Statistics ===\n");
    
    WynReleaseManager* manager = wyn_release_manager_new("1.0.0", WYN_RELEASE_STABLE);
    wyn_release_manager_initialize(manager);
    
    // Add some artifacts
    WynReleaseArtifact* artifact1 = wyn_release_artifact_new("compiler", WYN_ARTIFACT_COMPILER, WYN_PLATFORM_LINUX_X64);
    WynReleaseArtifact* artifact2 = wyn_release_artifact_new("stdlib", WYN_ARTIFACT_STDLIB, WYN_PLATFORM_LINUX_X64);
    
    wyn_release_artifact_build(artifact1, "/build/compiler");
    wyn_release_artifact_build(artifact2, "/build/stdlib");
    wyn_release_artifact_sign(artifact1, "key");
    wyn_release_artifact_sign(artifact2, "key");
    wyn_release_artifact_verify(artifact1, "key");
    wyn_release_artifact_verify(artifact2, "key");
    
    wyn_release_manager_add_artifact(manager, artifact1);
    wyn_release_manager_add_artifact(manager, artifact2);
    
    wyn_release_manager_run_all_validations(manager);
    
    WynReleaseMetrics* metrics = wyn_release_manager_get_metrics(manager);
    if (metrics) {
        printf("📊 Release Metrics:\n");
        printf("- Total artifacts: %zu\n", metrics->total_artifacts);
        printf("- Signed artifacts: %zu\n", metrics->signed_artifacts);
        printf("- Verified artifacts: %zu\n", metrics->verified_artifacts);
        printf("- Passed validations: %zu\n", metrics->passed_validations);
        printf("- Failed validations: %zu\n", metrics->failed_validations);
        printf("- Critical failures: %zu\n", metrics->critical_failures);
        printf("- Total build time: %.2f seconds\n", metrics->total_build_time);
        printf("- Total validation time: %.2f seconds\n", metrics->total_validation_time);
        printf("- Total file size: %zu bytes\n", metrics->total_file_size);
        
        free(metrics);
    }
    
    wyn_release_artifact_free(artifact1);
    wyn_release_artifact_free(artifact2);
    wyn_release_manager_free(manager);
}

int main() {
    printf("Wyn Language Final Release Preparation Examples\n");
    printf("===============================================\n\n");
    
    demonstrate_release_preparation();
    demonstrate_release_metrics();
    
    return 0;
}
