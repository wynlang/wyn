#!/bin/bash
# Container build and deployment script for Wyn Language

set -e

# Configuration
REGISTRY=${REGISTRY:-"wyn-lang"}
VERSION=${VERSION:-"latest"}
PLATFORM=${PLATFORM:-"linux/amd64,linux/arm64"}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')] $1${NC}"
}

warn() {
    echo -e "${YELLOW}[$(date +'%Y-%m-%d %H:%M:%S')] WARNING: $1${NC}"
}

error() {
    echo -e "${RED}[$(date +'%Y-%m-%d %H:%M:%S')] ERROR: $1${NC}"
    exit 1
}

# Build Docker images
build_images() {
    log "Building Wyn compiler Docker images..."
    
    # Build standard Ubuntu-based image
    log "Building standard image: ${REGISTRY}/compiler:${VERSION}"
    docker build -f docker/Dockerfile -t "${REGISTRY}/compiler:${VERSION}" .
    
    # Build minimal Alpine-based image
    log "Building minimal image: ${REGISTRY}/compiler:${VERSION}-alpine"
    docker build -f docker/Dockerfile.alpine -t "${REGISTRY}/compiler:${VERSION}-alpine" .
    
    log "Docker images built successfully"
}

# Test container functionality
test_containers() {
    log "Testing container functionality..."
    
    # Test standard image
    log "Testing standard image..."
    docker run --rm "${REGISTRY}/compiler:${VERSION}" --version || error "Standard image test failed"
    
    # Test Alpine image
    log "Testing Alpine image..."
    docker run --rm "${REGISTRY}/compiler:${VERSION}-alpine" --version || error "Alpine image test failed"
    
    log "Container tests passed"
}

# Deploy to Kubernetes
deploy_k8s() {
    log "Deploying to Kubernetes..."
    
    # Check if kubectl is available
    if ! command -v kubectl &> /dev/null; then
        error "kubectl not found. Please install kubectl to deploy to Kubernetes."
    fi
    
    # Apply Kubernetes manifests
    kubectl apply -f k8s/deployment.yaml
    
    # Wait for deployment to be ready
    log "Waiting for deployment to be ready..."
    kubectl wait --for=condition=available --timeout=300s deployment/wyn-compiler
    
    # Get service information
    kubectl get service wyn-compiler-service
    
    log "Kubernetes deployment completed"
}

# Push images to registry
push_images() {
    log "Pushing images to registry..."
    
    docker push "${REGISTRY}/compiler:${VERSION}"
    docker push "${REGISTRY}/compiler:${VERSION}-alpine"
    
    log "Images pushed successfully"
}

# Multi-platform build
build_multiplatform() {
    log "Building multi-platform images..."
    
    # Create buildx builder if it doesn't exist
    docker buildx create --name wyn-builder --use 2>/dev/null || true
    
    # Build and push multi-platform images
    docker buildx build --platform "${PLATFORM}" \
        -f docker/Dockerfile \
        -t "${REGISTRY}/compiler:${VERSION}" \
        --push .
    
    docker buildx build --platform "${PLATFORM}" \
        -f docker/Dockerfile.alpine \
        -t "${REGISTRY}/compiler:${VERSION}-alpine" \
        --push .
    
    log "Multi-platform build completed"
}

# Show usage
usage() {
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  build       Build Docker images locally"
    echo "  test        Test container functionality"
    echo "  push        Push images to registry"
    echo "  deploy      Deploy to Kubernetes"
    echo "  multiplatform Build and push multi-platform images"
    echo "  all         Build, test, and push images"
    echo ""
    echo "Environment variables:"
    echo "  REGISTRY    Docker registry (default: wyn-lang)"
    echo "  VERSION     Image version tag (default: latest)"
    echo "  PLATFORM    Target platforms for multiplatform build (default: linux/amd64,linux/arm64)"
}

# Main execution
case "${1:-}" in
    build)
        build_images
        ;;
    test)
        test_containers
        ;;
    push)
        push_images
        ;;
    deploy)
        deploy_k8s
        ;;
    multiplatform)
        build_multiplatform
        ;;
    all)
        build_images
        test_containers
        push_images
        ;;
    *)
        usage
        exit 1
        ;;
esac

log "Operation completed successfully"
