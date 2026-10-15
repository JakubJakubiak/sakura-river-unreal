#include "SakuraLayout.h"

namespace Sakura
{
	double Smooth(double A, double B, double X)
	{
		const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	static double Hash2(double X, double Y)
	{
		const double S = FMath::Sin(X * 127.1 + Y * 311.7) * 43758.5453;
		return S - FMath::FloorToDouble(S);
	}

	double Noise(double X, double Y)
	{
		const double XI = FMath::FloorToDouble(X), YI = FMath::FloorToDouble(Y);
		const double XF = X - XI, YF = Y - YI;
		const double U = XF * XF * (3 - 2 * XF), V = YF * YF * (3 - 2 * YF);
		const double A = Hash2(XI, YI), B = Hash2(XI + 1, YI), C = Hash2(XI, YI + 1), D = Hash2(XI + 1, YI + 1);
		return A + (B - A) * U + (C - A) * V + (A - B - C + D) * U * V;
	}

	double Fbm(double X, double Y, int32 Octaves)
	{
		double S = 0, A = 0.5, F = 1, N = 0;
		for (int32 I = 0; I < Octaves; ++I) { S += A * Noise(X * F, Y * F); N += A; A *= 0.5; F *= 2.03; }
		return S / N;
	}

	double Ridged(double X, double Y, int32 Octaves)
	{
		double S = 0, A = 0.5, F = 1, N = 0;
		for (int32 I = 0; I < Octaves; ++I)
		{
			const double R = 1.0 - FMath::Abs(Noise(X * F, Y * F) * 2 - 1);
			S += A * R * R; N += A; A *= 0.5; F *= 2.07;
		}
		return S / N;
	}

	double RiverCx(double Z)
	{
		return Z < -60 ? (Z + 60) * (Z + 60) * 0.012 : (Z > 36 ? -(Z - 36) * (Z - 36) * 0.03 : 0.0);
	}

	double RiverHalf(double Z) { return 12 + 2.2 * FMath::Sin(Z * 0.045) + 1.2 * FMath::Sin(Z * 0.11 + 1); }

	double RiverDist(double X, double Z) { return FMath::Abs(X - RiverCx(Z)) - RiverHalf(Z); }

	double GroundH(double X, double Z, bool bHD)
	{
		const double D = RiverDist(X, Z);
		double Water = -2.3 * FMath::Min(1.0, -D / 4.5) - 0.25 * Noise(X * 0.25, Z * 0.25) * Smooth(0, 3, -D);
		double Land = 0.9 * Smooth(0, 4, D) + Noise(X * 0.06 + 10, Z * 0.06 + 10) * 3.2 * Smooth(5, 45, D)
			+ Noise(X * 0.2, Z * 0.2) * 0.35 * Smooth(1, 6, D) + Smooth(35, 140, D) * 16;
		if (bHD)
		{
			Land += (Fbm(X * 0.7, Z * 0.7, 3) - 0.5) * 0.22 * Smooth(0.5, 3, D);
			Water += (Noise(X * 1.3, Z * 0.6) - 0.5) * 0.12;
		}
		return D < 0 ? Water : Land;
	}

	static double Mountains(double X, double Z)
	{
		const double Dc = FMath::Sqrt(X * X + (Z + 60.0) * (Z + 60.0));
		const double D = RiverDist(X, Z);
		const double R = Ridged(X / 1400.0 + 3.1, Z / 1400.0 - 7.7, 6);
		const double Valley = Smooth(40, 400, D);
		return (Smooth(1800, 6500, Dc) * (120 + 520 * R) + Smooth(250, 1600, Dc) * Fbm(X / 300.0, Z / 300.0, 4) * 40) * Valley;
	}

	double FarH(double X, double Z)
	{
		const double Ang = FMath::Abs(FMath::Atan2(X, -(Z - 27.0)));
		const double Corridor = 0.35 + 0.65 * Smooth(0.05, 0.5, Ang);
		return GroundH(X, Z, false) + Mountains(X, Z) * Corridor;
	}

	// siatki z Tools/blender/terrain.py
	struct FGrid { double X0, X1, Z0, Z1, Step; };
	static const FGrid Near = { -160.0, 160.0, -300.0, 110.0, 0.6 };
	static const FGrid Far = { -6000.0, 6000.0, -14000.0, 2000.0, 40.0 };

	template <typename FH>
	static double GridHeight(const FGrid& G, double X, double Z, FH&& HeightAt)
	{
		const int32 NX = FMath::FloorToInt32((G.X1 - G.X0) / G.Step + 0.5) + 1;
		const int32 NZ = FMath::FloorToInt32((G.Z1 - G.Z0) / G.Step + 0.5) + 1;
		const double GX = FMath::Clamp((X - G.X0) / G.Step, 0.0, NX - 1.0001);
		const double GZ = FMath::Clamp((Z - G.Z0) / G.Step, 0.0, NZ - 1.0001);
		const int32 I = FMath::FloorToInt32(GX), J = FMath::FloorToInt32(GZ);
		const double FX = GX - I, FZ = GZ - J;
		auto Node = [&](int32 A, int32 B) { return HeightAt(G.X0 + A * G.Step, G.Z0 + B * G.Step); };
		const double Ha = Node(I, J), Hb = Node(I + 1, J), Hc = Node(I, J + 1), Hd = Node(I + 1, J + 1);
		// trójkąty (a, c, b) i (b, c, d) — przekątna b–c, jak w grid_tris()
		if (FX + FZ <= 1.0) return Ha + (Hb - Ha) * FX + (Hc - Ha) * FZ;
		return Hd + (Hc - Hd) * (1.0 - FX) + (Hb - Hd) * (1.0 - FZ);
	}

	double TerrainHeight(double X, double Z)
	{
		if (X > Near.X0 + 0.7 && X < Near.X1 - 0.7 && Z > Near.Z0 + 0.7 && Z < Near.Z1 - 0.7)
		{
			return GridHeight(Near, X, Z, [](double A, double B) { return GroundH(A, B, true); });
		}
		return GridHeight(Far, X, Z, [](double A, double B)
		{
			double H = FarH(A, B);
			if (A > Near.X0 + 2 && A < Near.X1 - 2 && B > Near.Z0 + 2 && B < Near.Z1 - 2) H -= 2.5;
			return H;
		});
	}
}
