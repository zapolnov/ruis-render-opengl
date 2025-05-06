/*
ruis-render-opengl - OpenGL renderer

Copyright (C) 2012-2024  Ivan Gagis <igagis@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/* ================ LICENSE END ================ */

#include "context.hpp"

#include <utki/shared.hpp>

#include "shaders/shader_color.hpp"
#include "shaders/shader_color_pos_lum.hpp"
#include "shaders/shader_color_pos_tex.hpp"
#include "shaders/shader_color_pos_tex_alpha.hpp"
#include "shaders/shader_pos_clr.hpp"
#include "shaders/shader_pos_tex.hpp"

#include "frame_buffer.hpp"
#include "index_buffer.hpp"
#include "texture_2d.hpp"
#include "texture_cube.hpp"
#include "texture_depth.hpp"
#include "vertex_array.hpp"
#include "vertex_buffer.hpp"

using namespace ruis::render::opengl;

// TODO: remove commented code
// namespace {
// unsigned get_max_texture_size()
// {
// 	// the val variable is initialized via output argument, so no need to initialize
// 	// it here

// 	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
// 	GLint val;
// 	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &val);
// 	ASSERT(val > 0)
// 	return unsigned(val);
// }
// } // namespace

#ifdef DEBUG
namespace {
void GLAPIENTRY opengl_error_callback(
	GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* user_param
)
{
	std::cout << "OpenGL" << (type == GL_DEBUG_TYPE_ERROR ? " ERROR" : "") << ": " << message << std::endl;
}
} // namespace
#endif

context::context() :
	ruis::render::context(
		{.initial_matrix = ruis::matrix4()
							   // OpenGL identity matrix:
							   //   viewport edges: left = -1, right = 1, top = 1, bottom = -1
							   //   z-axis towards viewer
							   .set_identity()
							   // x-axis right, y-axis down, z-axis away
							   .scale(1, -1, -1)
							   // viewport edges: left = 0, top = 0
							   .translate(-1, -1)
							   // viewport edges: right = 1, bottom = 1
							   .scale(2, 2)}
	)
{
	LOG([](auto& o) {
		o << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
	})

	// On some platforms the default framebuffer is not 0, so because of this
	// check if default framebuffer value is saved or not everytime some
	// framebuffer is going to be bound and save the value if needed.

	// the old_fb variable is initialized via output argument, so no need to initialize
	// it here

	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	GLint old_fb;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fb);
	LOG([&](auto& o) {
		o << "old_fb = " << old_fb << std::endl;
	})
	this->default_framebuffer = GLuint(old_fb);

#ifdef DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(opengl_error_callback, nullptr);
#endif

	glEnable(GL_CULL_FACE);
}

utki::shared_ref<ruis::render::context::shaders> context::make_shaders()
{
	// TODO: are those lint supressions still valid?
	auto ret = utki::make_shared<ruis::render::context::shaders>();
	// NOLINTNEXTLINE(bugprone-unused-return-value, "false positive")
	ret.get().pos_tex = std::make_unique<shader_pos_tex>(this->get_shared_ref());
	// NOLINTNEXTLINE(bugprone-unused-return-value, "false positive")
	ret.get().color_pos = std::make_unique<shader_color>(this->get_shared_ref());
	// NOLINTNEXTLINE(bugprone-unused-return-value, "false positive")
	ret.get().pos_clr = std::make_unique<shader_pos_clr>(this->get_shared_ref());
	// NOLINTNEXTLINE(bugprone-unused-return-value, "false positive")
	ret.get().color_pos_tex = std::make_unique<shader_color_pos_tex>(this->get_shared_ref());
	// NOLINTNEXTLINE(bugprone-unused-return-value, "false positive")
	ret.get().color_pos_tex_alpha = std::make_unique<shader_color_pos_tex_alpha>(this->get_shared_ref());
	// NOLINTNEXTLINE(bugprone-unused-return-value, "false positive")
	ret.get().color_pos_lum = std::make_unique<shader_color_pos_lum>(this->get_shared_ref());
	return ret;
}

utki::shared_ref<ruis::render::texture_2d> context::make_texture_2d(
	rasterimage::format format,
	rasterimage::dimensioned::dimensions_type dims,
	texture_2d_parameters params
)
{
	return this->create_texture_2d_internal(format, dims, {}, std::move(params));
}

utki::shared_ref<ruis::render::texture_2d> context::make_texture_2d(
	const rasterimage::image_variant& imvar,
	texture_2d_parameters params
)
{
	auto imvar_copy = imvar;
	return this->make_texture_2d(std::move(imvar_copy), std::move(params));
}

utki::shared_ref<ruis::render::texture_2d> context::make_texture_2d(
	rasterimage::image_variant&& imvar,
	texture_2d_parameters params
)
{
	auto iv = std::move(imvar);
	return std::visit(
		[this, &imvar = iv, &params](auto&& im) -> utki::shared_ref<ruis::render::texture_2d> {
			if constexpr (sizeof(im.pixels().front().front()) != 1) {
				throw std::logic_error(
					"context::make_texture_2d(): "
					"non-8bit images are not supported"
				);
			} else {
				im.span().flip_vertical();
				auto data = im.pixels();
				return this->create_texture_2d_internal(
					imvar.get_format(),
					im.dims(),
					utki::make_span(data.front().data(), data.size_bytes()),
					std::move(params)
				);
			}
		},
		iv.variant
	);
}

utki::shared_ref<ruis::render::texture_2d> context::create_texture_2d_internal(
	rasterimage::format type,
	rasterimage::dimensioned::dimensions_type dims,
	utki::span<const uint8_t> data,
	texture_2d_parameters params
)
{
	return utki::make_shared<texture_2d>(
		this->get_shared_ref(), //
		type,
		dims,
		data,
		params
	);
}

utki::shared_ref<ruis::render::texture_depth> context::make_texture_depth(
	rasterimage::dimensioned::dimensions_type dims
)
{
	return utki::make_shared<texture_depth>(
		this->get_shared_ref(), //
		dims
	);
}

utki::shared_ref<ruis::render::texture_cube> context::make_texture_cube(
	rasterimage::image_variant&& positive_x,
	rasterimage::image_variant&& negative_x,
	rasterimage::image_variant&& positive_y,
	rasterimage::image_variant&& negative_y,
	rasterimage::image_variant&& positive_z,
	rasterimage::image_variant&& negative_z
)
{
	constexpr auto num_cube_sides = 6;
	std::array<rasterimage::image_variant, num_cube_sides> sides = {
		std::move(positive_x),
		std::move(negative_x),

		// negative_y and positive_y are swapped due to texture coordinates y-axis going downwards
		std::move(negative_y),
		std::move(positive_y),

		std::move(positive_z),
		std::move(negative_z)
	};
	std::array<ruis::render::opengl::texture_cube::cube_face_image, num_cube_sides> faces;

	auto face = faces.begin();
	for (auto& side : sides) {
		ASSERT(face != faces.end())
		std::visit(
			[&](auto& im) {
				if constexpr (sizeof(im.pixels().front().front()) != 1) {
					throw std::logic_error(
						"context::make_texture_cube(): "
						"non-8bit images are not supported"
					);
				} else {
					im.span().flip_vertical();
					auto data = im.pixels();

					face->type = side.get_format();
					face->dims = im.dims();
					face->data = utki::make_span(data.front().data(), data.size_bytes());
				}
			},
			side.variant
		);
		++face;
	}

	return utki::make_shared<texture_cube>(
		this->get_shared_ref(), //
		faces
	);
}

utki::shared_ref<ruis::render::vertex_buffer> context::make_vertex_buffer(utki::span<const r4::vector4<float>> vertices)
{
	return utki::make_shared<vertex_buffer>(
		this->get_shared_ref(), //
		vertices
	);
}

utki::shared_ref<ruis::render::vertex_buffer> context::make_vertex_buffer(utki::span<const r4::vector3<float>> vertices)
{
	return utki::make_shared<vertex_buffer>(
		this->get_shared_ref(), //
		vertices
	);
}

utki::shared_ref<ruis::render::vertex_buffer> context::make_vertex_buffer(utki::span<const r4::vector2<float>> vertices)
{
	return utki::make_shared<vertex_buffer>(
		this->get_shared_ref(), //
		vertices
	);
}

utki::shared_ref<ruis::render::vertex_buffer> context::make_vertex_buffer(utki::span<const float> vertices)
{
	return utki::make_shared<vertex_buffer>(
		this->get_shared_ref(), //
		vertices
	);
}

utki::shared_ref<ruis::render::vertex_array> context::make_vertex_array(
	std::vector<utki::shared_ref<const ruis::render::vertex_buffer>> buffers,
	utki::shared_ref<const ruis::render::index_buffer> indices,
	ruis::render::vertex_array::mode mode
)
{
	return utki::make_shared<vertex_array>(
		this->get_shared_ref(), //
		std::move(buffers),
		std::move(indices),
		mode
	);
}

utki::shared_ref<ruis::render::index_buffer> context::make_index_buffer(utki::span<const uint16_t> indices)
{
	return utki::make_shared<index_buffer>(
		this->get_shared_ref(), //
		indices
	);
}

utki::shared_ref<ruis::render::index_buffer> context::make_index_buffer(utki::span<const uint32_t> indices)
{
	return utki::make_shared<index_buffer>(
		this->get_shared_ref(), //
		indices
	);
}

utki::shared_ref<ruis::render::frame_buffer> context::make_framebuffer( //
	std::shared_ptr<ruis::render::texture_2d> color,
	std::shared_ptr<ruis::render::texture_depth> depth,
	std::shared_ptr<ruis::render::texture_stencil> stencil
)
{
	return utki::make_shared<frame_buffer>( //
		this->get_shared_ref(),
		std::move(color),
		std::move(depth),
		std::move(stencil)
	);
}

void context::set_framebuffer_internal(ruis::render::frame_buffer* fb)
{
	if (!fb) {
		glBindFramebuffer(GL_FRAMEBUFFER, this->default_framebuffer);
		assert_opengl_no_error();
		return;
	}

	ASSERT(dynamic_cast<frame_buffer*>(fb))
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
	auto& ogl_fb = static_cast<frame_buffer&>(*fb);

	glBindFramebuffer(GL_FRAMEBUFFER, ogl_fb.fbo);
	assert_opengl_no_error();
}

void context::clear_framebuffer_color()
{
	// Default clear color is RGBA = (0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	assert_opengl_no_error();
}

void context::clear_framebuffer_depth()
{
	// Default clear depth value is 1, see glClearDepth()
	glClear(GL_DEPTH_BUFFER_BIT);
	assert_opengl_no_error();
}

void context::clear_framebuffer_stencil()
{
	// Default clear stencil value is 0, see glClearStencil()
	glClear(GL_STENCIL_BUFFER_BIT);
	assert_opengl_no_error();
}

r4::vector2<uint32_t> context::to_window_coords(ruis::vec2 point) const
{
	auto vp = this->get_viewport();

	point += ruis::vec2(1, 1);
	point = max(point, {0, 0}); // clamp to >= 0
	point /= 2;
	point.comp_multiply(vp.d.to<real>());
	point = round(point);
	return point.to<uint32_t>() + vp.p;
}

bool context::is_scissor_enabled() const noexcept
{
	return glIsEnabled(GL_SCISSOR_TEST) ? true : false; // "? true : false" is to avoid warning under MSVC
}

void context::enable_scissor(bool enable)
{
	if (enable) {
		glEnable(GL_SCISSOR_TEST);
	} else {
		glDisable(GL_SCISSOR_TEST);
	}
}

r4::rectangle<uint32_t> context::get_scissor() const
{
	std::array<GLint, 4> osb{};
	glGetIntegerv(GL_SCISSOR_BOX, osb.data());

#ifdef DEBUG
	for (auto n : osb) {
		ASSERT(n >= 0)
	}
#endif

	return {
		uint32_t(osb[0]), //
		uint32_t(osb[1]),
		uint32_t(osb[2]),
		uint32_t(osb[3])
	};
}

void context::set_scissor(r4::rectangle<uint32_t> r)
{
	glScissor(GLint(r.p.x()), GLint(r.p.y()), GLint(r.d.x()), GLint(r.d.y()));
	assert_opengl_no_error();
}

r4::rectangle<uint32_t> context::get_viewport() const
{
	std::array<GLint, 4> vp{};

	glGetIntegerv(GL_VIEWPORT, vp.data());

#ifdef DEBUG
	for (auto n : vp) {
		ASSERT(n >= 0)
	}
#endif

	return {
		uint32_t(vp[0]), //
		uint32_t(vp[1]),
		uint32_t(vp[2]),
		uint32_t(vp[3])
	};
}

void context::set_viewport(r4::rectangle<uint32_t> r)
{
	glViewport(GLint(r.p.x()), GLint(r.p.y()), GLint(r.d.x()), GLint(r.d.y()));
	assert_opengl_no_error();
}

void context::enable_blend(bool enable)
{
	if (enable) {
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}
}

namespace {

const std::array<GLenum, size_t(ruis::render::context::blend_factor::enum_size)> blend_func = {
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_CONSTANT_COLOR,
	GL_ONE_MINUS_CONSTANT_COLOR,
	GL_CONSTANT_ALPHA,
	GL_ONE_MINUS_CONSTANT_ALPHA,
	GL_SRC_ALPHA_SATURATE
};

} // namespace

void context::set_blend_func(
	blend_factor src_color,
	blend_factor dst_color,
	blend_factor src_alpha,
	blend_factor dst_alpha
)
{
	glBlendFuncSeparate(
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
		blend_func[unsigned(src_color)],
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
		blend_func[unsigned(dst_color)],
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
		blend_func[unsigned(src_alpha)],
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
		blend_func[unsigned(dst_alpha)]
	);
}

bool context::is_depth_enabled() const noexcept
{
	return glIsEnabled(GL_DEPTH_TEST) ? true : false; // "? true : false" is to avoid warning under MSVC
}

void context::enable_depth(bool enable)
{
	if (enable) {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}
}
