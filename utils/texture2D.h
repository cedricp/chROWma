#pragma once

#include "GL/glew.h"

class Texture2D
{
	GLuint _texid;
	int _internal_format;
	// Forbid copy, you must use reference only to avoid garbage
	// ---------------------------------------------
	Texture2D(const Texture2D& t){}
	Texture2D& operator = (const Texture2D& t){return *this;}
	// ---------------------------------------------
protected:
	void _init(GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
						 GLint wrap_s, GLint wrap_t, GLint min_filter, GLint max_filter,const void *pixels);
public:
	Texture2D();
	Texture2D(GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
						 GLint wrap_s, GLint wrap_t, GLint min_filter, GLint max_filter,const void *pixels);
	virtual ~Texture2D();

	void update_buffer(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLint format, GLenum type, void* pixels);
	GLuint gl_texture() const {return _texid;}
	void* get_subdata_float_rgb(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,GLsizei height, GLint level = 0, GLsizei depth = 1);

	virtual void init(GLenum format, GLenum type, GLsizei width, GLsizei height,  const void *pixels = NULL, GLint border = 0,
			 GLint wrap_s = GL_CLAMP_TO_BORDER, GLint wrap_t = GL_CLAMP_TO_BORDER, GLint min_filter = GL_LINEAR, GLint max_filter = GL_LINEAR){
			 }
	int width() const;
	int height() const;

	void bindImageTexture(GLuint unit = 0, GLenum access = GL_READ_WRITE,GLint level = 0, GLboolean layered = GL_FALSE, GLint layer = 0) const
	{
		glBindImageTexture(unit, _texid, level, layered, layer, access, _internal_format);
	}

	void bindTexture(GLuint unit) const
	{
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, _texid);
	}

	void clear(GLenum format = GL_RGB, GLenum type = GL_FLOAT, void* pixels = NULL);
	void swap(Texture2D& s);
	void* to_cpu_ram(GLint format, GLenum type);
	GLenum internal_format(){return _internal_format;}
};

template <GLenum iformat>
class Texture_ : public Texture2D
{
	// Forbid copy, you must use reference only to avoid garbage
	Texture_(const Texture_& t){}
	Texture_& operator = (const Texture_& t){return *this;}
public:
	// ---------------------------------------------
	Texture_(GLenum format, GLenum type, GLsizei width, GLsizei height,  const void *pixels = NULL, GLint border = 0,
			 GLint wrap_s = GL_CLAMP_TO_BORDER, GLint wrap_t = GL_CLAMP_TO_BORDER, GLint min_filter = GL_LINEAR, GLint max_filter = GL_LINEAR) : Texture2D(0, iformat, width, height,
			 border, format, type, wrap_s, wrap_t, min_filter, max_filter, pixels)
	{

	}

	Texture_() : Texture2D(){}

	void init(GLenum format, GLenum type, GLsizei width, GLsizei height,  const void *pixels = NULL, GLint border = 0,
			 GLint wrap_s = GL_CLAMP_TO_BORDER, GLint wrap_t = GL_CLAMP_TO_BORDER, GLint min_filter = GL_LINEAR, GLint max_filter = GL_LINEAR) override
	{
		Texture2D::_init(0, iformat, width, height, border, format, type, wrap_s, wrap_t, min_filter, max_filter, pixels);
	}	
};

typedef Texture_<GL_RGBA16F> TextureRGBA16F;
typedef Texture_<GL_RGB16F> TextureRGB16F;
typedef Texture_<GL_RGB32F> TextureRGB32F;
typedef Texture_<GL_RG16F> TextureRG16F;
typedef Texture_<GL_R16F> TextureR16F;
typedef Texture_<GL_R32UI> TextureR32UI;
