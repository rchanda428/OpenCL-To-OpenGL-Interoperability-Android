<?xml version="1.0" encoding="UTF-8"?>
<!--
   Hyllian's 2xBR Shader
   
   Copyright (C) 2011 Hyllian/Jararaca - sergiogdb@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
-->

<shader language="GLSL">
<vertex><![CDATA[
	attribute vec2 aPosition;
	attribute vec2 aTexCoord;
	varying vec2 vTexCoord;

	void main() {
		vTexCoord = aTexCoord;
		gl_Position = vec4(aPosition, 0.0, 1.0);
	}
]]></vertex>

<fragment filter="nearest" output_width="200%" output_height="200%"><![CDATA[
	#ifdef GL_FRAGMENT_PRECISION_HIGH
	precision highp float;
	#else
	precision mediump float;
	#endif
    uniform sampler2D rubyTexture;
	varying vec2 vTexCoord;
	void main() {
			vec4  i = texture2D(rubyTexture, vTexCoord);
			gl_FragColor.rgba = texture2D(rubyTexture, vTexCoord);
		}
]]></fragment>
</shader>
