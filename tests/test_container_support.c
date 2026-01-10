#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Test container support functionality
static int test_docker_build() {
    printf("Testing Docker build capability...\n");
    
    // Check if Docker is available
    int result = system("docker --version > /dev/null 2>&1");
    if (result != 0) {
        printf("  SKIP: Docker not available\n");
        return 1; // Skip test
    }
    
    // Test building standard Dockerfile
    printf("  Building standard Docker image...\n");
    result = system("cd .. && docker build -f docker/Dockerfile -t wyn-test:standard . > /dev/null 2>&1");
    if (result != 0) {
        printf("  FAIL: Standard Docker build failed\n");
        return 0;
    }
    
    // Test building Alpine Dockerfile
    printf("  Building Alpine Docker image...\n");
    result = system("cd .. && docker build -f docker/Dockerfile.alpine -t wyn-test:alpine . > /dev/null 2>&1");
    if (result != 0) {
        printf("  FAIL: Alpine Docker build failed\n");
        return 0;
    }
    
    printf("  PASS: Docker builds successful\n");
    return 1;
}

static int test_container_functionality() {
    printf("Testing container functionality...\n");
    
    // Test standard container
    printf("  Testing standard container...\n");
    int result = system("docker run --rm wyn-test:standard --version > /dev/null 2>&1");
    if (result != 0) {
        printf("  FAIL: Standard container test failed\n");
        return 0;
    }
    
    // Test Alpine container
    printf("  Testing Alpine container...\n");
    result = system("docker run --rm wyn-test:alpine --version > /dev/null 2>&1");
    if (result != 0) {
        printf("  FAIL: Alpine container test failed\n");
        return 0;
    }
    
    printf("  PASS: Container functionality tests passed\n");
    return 1;
}

static int test_kubernetes_manifests() {
    printf("Testing Kubernetes manifest validation...\n");
    
    // Check if kubectl is available
    int result = system("kubectl version --client > /dev/null 2>&1");
    if (result != 0) {
        printf("  SKIP: kubectl not available\n");
        return 1; // Skip test
    }
    
    // Validate Kubernetes manifests
    printf("  Validating deployment manifest...\n");
    result = system("kubectl apply --dry-run=client -f ../k8s/deployment.yaml > /dev/null 2>&1");
    if (result != 0) {
        printf("  FAIL: Kubernetes manifest validation failed\n");
        return 0;
    }
    
    printf("  PASS: Kubernetes manifests are valid\n");
    return 1;
}

static int test_docker_compose() {
    printf("Testing Docker Compose configuration...\n");
    
    // Check if docker-compose is available
    int result = system("docker-compose --version > /dev/null 2>&1");
    if (result != 0) {
        printf("  SKIP: docker-compose not available\n");
        return 1; // Skip test
    }
    
    // Validate docker-compose.yml
    printf("  Validating docker-compose.yml...\n");
    result = system("cd .. && docker-compose config > /dev/null 2>&1");
    if (result != 0) {
        printf("  FAIL: docker-compose.yml validation failed\n");
        return 0;
    }
    
    printf("  PASS: Docker Compose configuration is valid\n");
    return 1;
}

static int test_deployment_script() {
    printf("Testing deployment script...\n");
    
    // Check if deployment script exists and is executable
    if (access("../scripts/container-deploy.sh", X_OK) != 0) {
        printf("  FAIL: Deployment script not found or not executable\n");
        return 0;
    }
    
    // Test script help functionality
    printf("  Testing script help...\n");
    int result = system("../scripts/container-deploy.sh > /dev/null 2>&1");
    if (result == 0) {
        printf("  FAIL: Script should exit with error for no arguments\n");
        return 0;
    }
    
    printf("  PASS: Deployment script is functional\n");
    return 1;
}

static int test_container_security() {
    printf("Testing container security configuration...\n");
    
    // Check if containers run as non-root user
    printf("  Checking non-root user configuration...\n");
    int result = system("docker run --rm wyn-test:standard id -u 2>/dev/null | grep -q '^1000$'");
    if (result != 0) {
        printf("  FAIL: Container not running as non-root user\n");
        return 0;
    }
    
    printf("  PASS: Container security configuration is correct\n");
    return 1;
}

int main() {
    printf("=== T6.2.2: Container Support Testing ===\n\n");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Run all tests
    total_tests++; if (test_docker_build()) passed_tests++;
    total_tests++; if (test_container_functionality()) passed_tests++;
    total_tests++; if (test_kubernetes_manifests()) passed_tests++;
    total_tests++; if (test_docker_compose()) passed_tests++;
    total_tests++; if (test_deployment_script()) passed_tests++;
    total_tests++; if (test_container_security()) passed_tests++;
    
    // Print summary
    printf("\n=== Container Support Test Summary ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    
    if (passed_tests == total_tests) {
        printf("✅ All container support tests passed!\n");
        printf("🐳 Docker and Kubernetes deployment ready\n");
        return 0;
    } else {
        printf("❌ Some container support tests failed\n");
        return 1;
    }
}
