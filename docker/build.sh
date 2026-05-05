#!/usr/bin/env bash

# Copyright (c) 2026 Abe Kohandel
# SPDX-License-Identifier: MIT

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Environment variables with default values
AWS_PROFILE="${AWS_PROFILE:-kodebooth-admin}"
AWS_REGION="${AWS_DEFAULT_REGION:-us-west-2}"
IMAGE_NAME="ghcr.io/kodebooth/qnx-virtio"
IMAGE_TAG="0.0.1"

echo "Using AWS profile: ${AWS_PROFILE}"
echo "Using AWS region: ${AWS_REGION}"

get_aws_credentials() {
  if ! command -v aws &>/dev/null; then
    echo "Error: AWS CLI is not installed"
    exit 1
  fi

  echo "Retrieving AWS credentials..."

  # Test if credentials are available and valid
  if ! aws sts get-caller-identity --profile "${AWS_PROFILE}" &>/dev/null; then
    echo "Error: Unable to authenticate with AWS"
    echo "If using SSO, please run: aws sso login --profile ${AWS_PROFILE}"
    exit 1
  fi

  # Extract temporary credentials
  ACCESS_KEY=$(aws configure get aws_access_key_id --profile "${AWS_PROFILE}" 2>/dev/null || echo "")
  SECRET_KEY=$(aws configure get aws_secret_access_key --profile "${AWS_PROFILE}" 2>/dev/null || echo "")
  SESSION_TOKEN=$(aws configure get aws_session_token --profile "${AWS_PROFILE}" 2>/dev/null || echo "")

  # If direct get doesn't work (SSO case), use credential process
  if [ -z "${ACCESS_KEY}" ]; then
    echo "Fetching SSO credentials..."
    CREDS=$(aws configure export-credentials --profile "${AWS_PROFILE}" --output json 2>/dev/null || echo "")

    if [ -n "${CREDS}" ]; then
      ACCESS_KEY=$(echo "${CREDS}" | grep -o '"AccessKeyId": "[^"]*"' | cut -d'"' -f4)
      SECRET_KEY=$(echo "${CREDS}" | grep -o '"SecretAccessKey": "[^"]*"' | cut -d'"' -f4)
      SESSION_TOKEN=$(echo "${CREDS}" | grep -o '"SessionToken": "[^"]*"' | cut -d'"' -f4)
    fi
  fi

  if [ -z "${ACCESS_KEY}" ] || [ -z "${SECRET_KEY}" ]; then
    echo "Error: Could not retrieve AWS credentials"
    echo "Please ensure you are logged in with: aws sso login --profile ${AWS_PROFILE}"
    exit 1
  fi

  echo "✓ AWS credentials retrieved successfully"
}

get_aws_credentials

# Export for Docker secrets
export ACCESS_KEY
export SECRET_KEY
export SESSION_TOKEN
export AWS_REGION

echo "Building Docker image from ${SCRIPT_DIR}..."
DOCKER_BUILDKIT=1 docker build \
  --secret id=aws_access_key_id,env=ACCESS_KEY \
  --secret id=aws_secret_access_key,env=SECRET_KEY \
  --secret id=aws_session_token,env=SESSION_TOKEN \
  --secret id=aws_region,env=AWS_REGION \
  -t "${IMAGE_NAME}:${IMAGE_TAG}" \
  -t "${IMAGE_NAME}:latest" \
  "${SCRIPT_DIR}"

echo "✓ Build complete"
