/*
 * Modern effects for a modern Streamer
 * Copyright (C) 2017 Michael Fabian Dirks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "gs-texture.hpp"
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include "obs/gs/gs-helper.hpp"

static uint32_t decode_flags(streamfx::obs::gs::texture::flags texture_flags)
{
	uint32_t flags = 0;
	if (has(texture_flags, streamfx::obs::gs::texture::flags::Dynamic))
		flags |= GS_DYNAMIC;
	if (has(texture_flags, streamfx::obs::gs::texture::flags::BuildMipMaps))
		flags |= GS_BUILD_MIPMAPS;
	if (has(texture_flags, streamfx::obs::gs::texture::flags::Shared))
		flags |= GS_SHARED_TEX;
	if (has(texture_flags, streamfx::obs::gs::texture::flags::GlobalShared))
		flags |= GS_SHARED_KM_TEX;
	return flags;
}

streamfx::obs::gs::texture::texture(uint32_t width, uint32_t height, gs_color_format format, uint32_t mip_levels,
									const uint8_t** mip_data, streamfx::obs::gs::texture::flags texture_flags)
{
	if (width == 0)
		throw std::logic_error("width must be at least 1");
	if (height == 0)
		throw std::logic_error("height must be at least 1");
	if (mip_levels == 0)
		throw std::logic_error("mip_levels must be at least 1");

	if (mip_levels > 1 || ((texture_flags & flags::BuildMipMaps) == flags::BuildMipMaps)) {
		bool isPOT = streamfx::util::math::is_power_of_two(width) && streamfx::util::math::is_power_of_two(height);
		if (!isPOT)
			throw std::logic_error("mip mapping requires power of two dimensions");
	}

	{
		auto gctx = streamfx::obs::gs::context();
		_texture  = gs_texture_create(width, height, format, mip_levels, mip_data, decode_flags(texture_flags));
	}

	if (!_texture)
		throw std::runtime_error("Failed to create texture.");

	_type = type::Normal;
}

streamfx::obs::gs::texture::texture(uint32_t width, uint32_t height, uint32_t depth, gs_color_format format,
									uint32_t mip_levels, const uint8_t** mip_data,
									streamfx::obs::gs::texture::flags texture_flags)
{
	if (width == 0)
		throw std::logic_error("width must be at least 1");
	if (height == 0)
		throw std::logic_error("height must be at least 1");
	if (depth == 0)
		throw std::logic_error("depth must be at least 1");
	if (mip_levels == 0)
		throw std::logic_error("mip_levels must be at least 1");

	if (mip_levels > 1 || ((texture_flags & flags::BuildMipMaps) == flags::BuildMipMaps)) {
		bool isPOT =
			(streamfx::util::math::is_equal(pow(2, static_cast<int64_t>(floor(log(width) / log(2)))), width)
			 && streamfx::util::math::is_equal(pow(2, static_cast<int64_t>(floor(log(height) / log(2)))), height)
			 && streamfx::util::math::is_equal(pow(2, static_cast<int64_t>(floor(log(depth) / log(2)))), depth));
		if (!isPOT)
			throw std::logic_error("mip mapping requires power of two dimensions");
	}

	{
		auto gctx = streamfx::obs::gs::context();
		_texture =
			gs_voltexture_create(width, height, depth, format, mip_levels, mip_data, decode_flags(texture_flags));
	}

	if (!_texture)
		throw std::runtime_error("Failed to create texture.");

	_type = type::Volume;
}

streamfx::obs::gs::texture::texture(uint32_t size, gs_color_format format, uint32_t mip_levels,
									const uint8_t** mip_data, streamfx::obs::gs::texture::flags texture_flags)
{
	if (size == 0)
		throw std::logic_error("size must be at least 1");
	if (mip_levels == 0)
		throw std::logic_error("mip_levels must be at least 1");

	if (mip_levels > 1 || ((texture_flags & flags::BuildMipMaps) == flags::BuildMipMaps)) {
		bool isPOT = streamfx::util::math::is_equal(pow(2, static_cast<int64_t>(floor(log(size) / log(2)))), size);
		if (!isPOT)
			throw std::logic_error("mip mapping requires power of two dimensions");
	}

	{
		auto gctx = streamfx::obs::gs::context();
		_texture  = gs_cubetexture_create(size, format, mip_levels, mip_data, decode_flags(texture_flags));
	}

	if (!_texture)
		throw std::runtime_error("Failed to create texture.");

	_type = type::Cube;
}

streamfx::obs::gs::texture::texture(std::string file)
{
	struct stat st;
	if (os_stat(file.c_str(), &st) != 0)
		throw std::ios_base::failure(file);

	auto gctx = streamfx::obs::gs::context();
	_texture  = gs_texture_create_from_file(file.c_str());

	if (!_texture)
		throw std::runtime_error("Failed to load texture.");
}

streamfx::obs::gs::texture::~texture()
{
	if (_is_owner && _texture) {
		auto gctx = streamfx::obs::gs::context();
		switch (gs_get_texture_type(_texture)) {
		case GS_TEXTURE_2D:
			gs_texture_destroy(_texture);
			break;
		case GS_TEXTURE_3D:
			gs_voltexture_destroy(_texture);
			break;
		case GS_TEXTURE_CUBE:
			gs_cubetexture_destroy(_texture);
			break;
		}
	}
	_texture = nullptr;
}

void streamfx::obs::gs::texture::load(int32_t unit)
{
	auto gctx = streamfx::obs::gs::context();
	gs_load_texture(_texture, unit);
}

gs_texture_t* streamfx::obs::gs::texture::get_object()
{
	return _texture;
}

uint32_t streamfx::obs::gs::texture::get_width()
{
	switch (_type) {
	case type::Normal:
		return gs_texture_get_width(_texture);
	case type::Volume:
		return gs_voltexture_get_width(_texture);
	case type::Cube:
		return gs_cubetexture_get_size(_texture);
	}
	return 0;
}

uint32_t streamfx::obs::gs::texture::get_height()
{
	switch (_type) {
	case type::Normal:
		return gs_texture_get_height(_texture);
	case type::Volume:
		return gs_voltexture_get_height(_texture);
	case type::Cube:
		return gs_cubetexture_get_size(_texture);
	}
	return 0;
}

uint32_t streamfx::obs::gs::texture::get_depth()
{
	switch (_type) {
	case type::Normal:
		return 1;
	case type::Volume:
		return gs_voltexture_get_depth(_texture);
	case type::Cube:
		return 6;
	}
	return 0;
}

streamfx::obs::gs::texture::type streamfx::obs::gs::texture::get_type()
{
	return _type;
}

gs_color_format streamfx::obs::gs::texture::get_color_format()
{
	return gs_texture_get_color_format(_texture);
}
