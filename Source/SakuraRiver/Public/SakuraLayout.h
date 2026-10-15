// Plan sceny Sakura River HD — te same wzory co w wersji web (index.html) i w generatorach Blendera (Tools/blender/common.py).
// Wysokość terenu liczona jest dokładnie tak, jak powstała siatka (te same węzły i ten sam podział na trójkąty),
// więc drzewa i kamienie stoją na ziemi bez śledzenia promieni.
#pragma once

#include "CoreMinimal.h"

namespace Sakura
{
	/** web (x w prawo, y w górę, z do kamery) w metrach → UE w cm */
	inline FVector W(double X, double Y, double Z) { return FVector(-Z * 100.0, X * 100.0, Y * 100.0); }
	/** UE (cm) → web (x, z) w metrach */
	inline FVector2D ToWeb(const FVector& P) { return FVector2D(P.Y / 100.0, -P.X / 100.0); }

	double Smooth(double A, double B, double X);
	double Noise(double X, double Y);
	double Fbm(double X, double Y, int32 Octaves);
	double Ridged(double X, double Y, int32 Octaves);

	double RiverCx(double Z);
	double RiverHalf(double Z);
	/** odległość od brzegu (ujemna w wodzie) */
	double RiverDist(double X, double Z);

	double GroundH(double X, double Z, bool bHD = true);
	double FarH(double X, double Z);
	/** Wysokość siatki terenu (bliskiej albo dalekiej) w punkcie web (x, z), w metrach. */
	double TerrainHeight(double X, double Z);

	/** łuk mostu (taiko-bashi) */
	inline double ArcY(double X) { return 0.9 + 4.0 * (1.0 - FMath::Square(X / 13.0)); }
	constexpr double BridgeZ = -24.0;

	/** pagoda: miejsce, obrót (rad) i skala jak w wersji web */
	constexpr double PagodaX = -22.0, PagodaZ = -46.0, PagodaRot = 0.35, PagodaScale = 1.15;
}
