/*
 * BuildInfo.h
 */

#pragma once

#define BUILDINFO_REPO_SHA 0x${build_info["repo_sha"]}U
#define BUILDINFO_REPO_SHA_CRC ${build_info["repo_sha_crc"]}U
#define BUILDINFO_REPO_DIRTY ${str(build_info["repo_dirty"]).lower()}
