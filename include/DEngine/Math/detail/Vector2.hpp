#pragma once

#include "DEngine/FixedWidthTypes.hpp"

namespace DEngine::Math
{
	template<uSize length, typename T>
	struct Vector;

	using Vec2 = Vector<2, f32>;
	using Vec2Int = Vector<2, i32>;

	template<>
	struct Vector<2, f32>
	{
		f32 x;
		f32 y;

		[[nodiscard]] constexpr Vector<3, f32> AsVec3(f32 zValue = 0.f) const;
		[[nodiscard]] constexpr Vector<4, f32> AsVec4(f32 zValue = 0.f, f32 wValue = 0.f) const;

		[[nodiscard]] f32 Magnitude() const;
		[[nodiscard]] constexpr f32 MagnitudeSqrd() const;
		[[nodiscard]] Vector<2, f32> Normalized() const;
		void Normalize();

		[[nodiscard]] constexpr f32* Data();
		[[nodiscard]] constexpr f32 const* Data() const;

		[[nodiscard]] static constexpr f32 Dot(Vector<2, f32> const& lhs, Vector<2, f32> const& rhs);

		[[nodiscard]] static constexpr Vector<2, f32> SingleValue(const f32& input);
		[[nodiscard]] static constexpr Vector<2, f32> Zero();
		[[nodiscard]] static constexpr Vector<2, f32> One();
		[[nodiscard]] static constexpr Vector<2, f32> Up();
		[[nodiscard]] static constexpr Vector<2, f32> Down();
		[[nodiscard]] static constexpr Vector<2, f32> Left();
		[[nodiscard]] static constexpr Vector<2, f32> Right();

		constexpr Vector<2, f32>& operator+=(const Vector<2, f32>& rhs)
		{
			x += rhs.x;
			y += rhs.y;
			return *this;
		}
		constexpr Vector<2, f32>& operator-=(const Vector<2, f32>& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;
			return *this;
		}
		constexpr Vector<2, f32>& operator*=(const f32& rhs)
		{
			x *= rhs;
			y *= rhs;
			return *this;
		}
		[[nodiscard]] constexpr Vector<2, f32> operator+(const Vector<2, f32>& rhs) const
		{
			return Vector<2, f32>{ x + rhs.x, y + rhs.y };
		}
		[[nodiscard]] constexpr Vector<2, f32> operator-(const Vector<2, f32>& rhs) const
		{
			return Vector<2, f32>{ x - rhs.x, y - rhs.y };
		}
		[[nodiscard]] constexpr Vector<2, f32> operator-() const
		{
			return Vector<2, f32>{ -x, -y };
		}
		[[nodiscard]] constexpr bool operator==(const Vector<2, f32>& rhs) const
		{
			return x == rhs.x && y == rhs.y;
		}
		[[nodiscard]] constexpr bool operator!=(const Vector<2, f32>& rhs) const
		{
			return x != rhs.x || y != rhs.y;
		}
	};
}