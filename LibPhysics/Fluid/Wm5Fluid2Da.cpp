// Geometric Tools, LLC
// Copyright (c) 1998-2014
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
//
// File Version: 5.0.1 (2010/10/01)

#include "Wm5PhysicsPCH.h"
#include "Wm5Fluid2Da.h"
#include "Wm5Memory.h"

namespace Wm5
{
//----------------------------------------------------------------------------
template <typename Real>
Fluid2Da<Real>::Fluid2Da (Real x0, Real y0, Real x1, Real y1, Real dt,
    Real denViscosity, Real velViscosity, int imax, int jmax,
    int numGaussSeidelIterations, bool densityDirichlet)
    :
    mX0(x0),
    mY0(y0),
    mX1(x1),
    mY1(y1),
    mDt(dt),
    mDenViscosity(denViscosity),
    mVelViscosity(velViscosity),
    mIMax(imax),
    mJMax(jmax),
    mNumGaussSeidelIterations(numGaussSeidelIterations),
    mDensityDirichlet(densityDirichlet)
{
    mIMaxM1 = mIMax - 1;
    mJMaxM1 = mJMax - 1;
    mIMaxP1 = mIMax + 1;
    mJMaxP1 = mJMax + 1;
    mNumPixels = mIMaxP1*mJMaxP1;
    mDx = (mX1 - mX0)/(Real)mIMax;
    mDy = (mY1 - mY0)/(Real)mJMax;
    mDxDx = mDx*mDx;
    mDyDy = mDy*mDy;
    mHalfDivDx = ((Real)0.5)/mDx;
    mHalfDivDy = ((Real)0.5)/mDy;
    mDtDivDx = mDt/mDx;
    mDtDivDy = mDt/mDy;
    mDtDivDxDx = mDt/mDxDx;
    mDtDivDyDy = mDt/mDyDy;
    mEpsilon0 = ((Real)0.5)*mDxDx*mDyDy/(mDxDx + mDyDy);
    mEpsilonX = mEpsilon0/mDxDx;
    mEpsilonY = mEpsilon0/mDyDy;
    mDenLambdaX = mDenViscosity*mDtDivDxDx;
    mDenLambdaY = mDenViscosity*mDtDivDyDy;
    mVelLambdaX = mVelViscosity*mDtDivDxDx;
    mVelLambdaY = mVelViscosity*mDtDivDyDy;
    mDenGamma0 = ((Real)1)/((Real)1 + ((Real)2)*(mDenLambdaX + mDenLambdaY));
    mDenGammaX = mDenLambdaX*mDenGamma0;
    mDenGammaY = mDenLambdaY*mDenGamma0;
    mVelGamma0 = ((Real)1)/((Real)1 + ((Real)2)*(mVelLambdaX + mVelLambdaY));
    mVelGammaX = mVelLambdaX*mVelGamma0;
    mVelGammaY = mVelLambdaY*mVelGamma0;
    mTime = (Real)0;

    mX = new1<Real>(mIMaxP1);
    mY = new1<Real>(mJMaxP1);
    mDensity0 = new2<Real>(mIMaxP1, mJMaxP1);
    mDensity1 = new2<Real>(mIMaxP1, mJMaxP1);
    mVelocity0 = new2<Vector2<Real> >(mIMaxP1, mJMaxP1);
    mVelocity1 = new2<Vector2<Real> >(mIMaxP1, mJMaxP1);
    mDivergence = new2<Real>(mIMaxP1, mJMaxP1);
    mPoisson = new2<Real>(mIMaxP1, mJMaxP1);

    for (int i = 0; i <= mIMax; ++i)
    {
        mX[i] = mX0 + mDx*(Real)i;
    }
    for (int j = 0; j <= mJMax; ++j)
    {
        mY[j] = mY0 + mDy*(Real)j;
    }

    size_t numBytes = mNumPixels*sizeof(Real);
    memset(mDensity0[0], 0, numBytes);
    memset(mDensity1[0], 0, numBytes);
    memset(mDivergence[0], 0, numBytes);
    memset(mPoisson[0], 0, numBytes);

    numBytes = mNumPixels*sizeof(Vector2<Real>);
    memset(mVelocity0[0], 0, numBytes);
    memset(mVelocity1[0], 0, numBytes);
}
//----------------------------------------------------------------------------
template <typename Real>
Fluid2Da<Real>::~Fluid2Da ()
{
    delete1(mX);
    delete1(mY);
    delete2(mDensity0);
    delete2(mDensity1);
    delete2(mVelocity0);
    delete2(mVelocity1);
    delete2(mDivergence);
    delete2(mPoisson);
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::Initialize ()
{
    for (int j = 1; j < mJMax; ++j)
    {
        for (int i = 1; i < mIMax; ++i)
        {
            mDensity1[j][i] = InitialDensity(mX[i], mY[j], i, j);
            mVelocity1[j][i] = InitialVelocity(mX[i], mY[j], i, j);
        }
    }

    UpdateDensityBoundary();
    UpdateVelocityBoundary();
    AdjustVelocity();

    size_t numBytes = mNumPixels*sizeof(Real);
    memcpy(mDensity0[0], mDensity1[0], numBytes);
    numBytes = mNumPixels*sizeof(Vector2<Real>);
    memcpy(mVelocity0[0][0], mVelocity1[0][0], numBytes);
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::DoSimulationStep ()
{
    UpdateDensitySource();
    UpdateDensityDiffusion();
    UpdateDensityAdvection();

    UpdateVelocitySource();
    UpdateVelocityDiffusion();
    UpdateVelocityAdvection();

    mTime += mDt;
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::UpdateDensitySource ()
{
    for (int j = 1; j < mJMax; ++j)
    {
        for (int i = 1; i < mIMax; ++i)
        {
            mDensity1[j][i] = mDensity0[j][i] +
                mDt*SourceDensity(mTime, mX[i], mY[j], i, j);

            if (mDensity1[j][i] < (Real)0)
            {
                mDensity1[j][i] = (Real)0;
            }
        }
    }

    UpdateDensityBoundary();
    SwapDensityBuffers();
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::UpdateDensityDiffusion ()
{
    for (int iter = 0; iter < mNumGaussSeidelIterations; ++iter)
    {
        for (int j = 1; j < mJMax; ++j)
        {
            for (int i = 1; i < mIMax; ++i)
            {
                Real sumX = mDensity1[j][i+1] + mDensity1[j][i-1];
                Real sumY = mDensity1[j+1][i] + mDensity1[j-1][i];

                mDensity1[j][i] = mDenGamma0*mDensity0[j][i] +
                    mDenGammaX*sumX + mDenGammaY*sumY;
            }
        }
        UpdateDensityBoundary();
    }
    SwapDensityBuffers();
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::UpdateDensityAdvection ()
{
    for (int j = 1; j < mJMax; ++j)
    {
        for (int i = 1; i < mIMax; ++i)
        {
            int i0, i1, j0, j1;
            Real a0, a1, b0, b1;
            GetLerpInfo(i, j, i0, i1, a0, a1, j0, j1, b0, b1);

            Real d00 = mDensity0[j0][i0];
            Real d10 = mDensity0[j0][i1];
            Real d01 = mDensity0[j1][i0];
            Real d11 = mDensity0[j1][i1];

            mDensity1[j][i] = b0*(a0*d00 + a1*d10) + b1*(a0*d01 + a1*d11);
        }
    }

    UpdateDensityBoundary();
    SwapDensityBuffers();
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::UpdateVelocitySource ()
{
    for (int j = 1; j < mJMax; ++j)
    {
        for (int i = 1; i < mIMax; ++i)
        {
            mVelocity1[j][i] = mVelocity0[j][i] +
                mDt*SourceVelocity(mTime, mX[i], mY[j], i, j);
        }
    }

    AdjustVelocity();
    SwapVelocityBuffers();
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::UpdateVelocityDiffusion ()
{
    for (int iter = 0; iter < mNumGaussSeidelIterations; ++iter)
    {
        for (int j = 1; j < mJMax; ++j)
        {
            for (int i = 1; i < mIMax; ++i)
            {
                mVelocity1[j][i] = mVelGamma0*mVelocity0[j][i] +
                    mVelGammaX*(mVelocity1[j][i+1] + mVelocity1[j][i-1]) +
                    mVelGammaY*(mVelocity1[j+1][i] + mVelocity1[j-1][i]);
            }
        }
        UpdateVelocityBoundary();
    }

    AdjustVelocity();
    SwapVelocityBuffers();
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::UpdateVelocityAdvection ()
{
    for (int j = 1; j < mJMax; ++j)
    {
        for (int i = 1; i < mIMax; ++i)
        {
            int i0, i1, j0, j1;
            Real a0, a1, b0, b1;
            GetLerpInfo(i, j, i0, i1, a0, a1, j0, j1, b0, b1);

            Vector2<Real> v00 = mVelocity0[j0][i0];
            Vector2<Real> v10 = mVelocity0[j0][i1];
            Vector2<Real> v01 = mVelocity0[j1][i0];
            Vector2<Real> v11 = mVelocity0[j1][i1];

            mVelocity1[j][i] = b0*(a0*v00 + a1*v10) + b1*(a0*v01 + a1*v11);
        }
    }

    AdjustVelocity();
    SwapVelocityBuffers();
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::GetLerpInfo (int i, int j, int& i0, int& i1, Real& a0,
    Real& a1, int& j0, int& j1, Real& b0, Real& b1)
{
    Real iPrevious = i - mDtDivDx*mVelocity0[j][i][0];
    if (iPrevious < (Real)0.5)
    {
        iPrevious = (Real)0.5;
    }
    else if (iPrevious > (Real)mIMax - (Real)0.5)
    {
        iPrevious = (Real)mIMax - (Real)0.5;
    }

    i0 = (int)Math<Real>::Floor(iPrevious);
    i1 = i0 + 1;
    a1 = iPrevious - i0;
    a0 = (Real)1 - a1;

    Real jPrevious = j - mDtDivDy*mVelocity0[j][i][1];
    if (jPrevious < (Real)0.5)
    {
        jPrevious = (Real)0.5;
    }
    else if (jPrevious > (Real)mJMax - (Real)0.5)
    {
        jPrevious = (Real)mJMax - (Real)0.5;
    }

    j0 = (int)Math<Real>::Floor(jPrevious);
    j1 = j0 + 1;
    b1 = jPrevious - j0;
    b0 = (Real)1 - b1;
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::AdjustVelocity ()
{
    int i, j;

    // Approximate the divergence of velocity.
    for (j = 1; j < mJMax; ++j)
    {
        for (i = 1; i < mIMax; ++i)
        {
            Real diffX = mVelocity1[j][i+1][0] - mVelocity1[j][i-1][0];
            Real diffY = mVelocity1[j+1][i][1] - mVelocity1[j-1][i][1];

            mDivergence[j][i] = mHalfDivDx*diffX + mHalfDivDy*diffY;
        }
    }

    // Use zero-valued derivative on boundary to assign divergence.
    NeumannBoundaryZero(mDivergence);

    // Initialize Poisson solution.
    memset(mPoisson[0], 0, mNumPixels*sizeof(Real));

    // Numerically solve Poisson?s equation. The boundary values remain zero,
    // so there is no need to call a boundary update function.
    for (int iter = 0; iter < mNumGaussSeidelIterations; ++iter)
    {
        for (j = 1; j < mJMax; ++j)
        {
            for (i = 1; i < mIMax; ++i)
            {
                Real sumX = mPoisson[j][i+1] + mPoisson[j][i-1];
                Real sumY = mPoisson[j+1][i] + mPoisson[j-1][i];

                mPoisson[j][i] = mEpsilon0*mDivergence[j][i] +
                    mEpsilonX*sumX + mEpsilonY*sumY;
            }
        }
    }

    // Adjust the velocity v? = v + gradient(poisson).
    for (j = 1; j < mJMax; ++j)
    {
        for (i = 1; i < mIMax; ++i)
        {
            Real diffX = mPoisson[j][i+1] - mPoisson[j][i-1];
            Real diffY = mPoisson[j+1][i] - mPoisson[j-1][i];

            mVelocity1[j][i][0] += mHalfDivDx*diffX;
            mVelocity1[j][i][1] += mHalfDivDy*diffY;
        }
    }

    UpdateVelocityBoundary();
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::DirichletBoundaryZero (Real** data)
{
    // x-edge-interior
    for (int j = 1; j < mJMax; ++j)
    {
        // data = 0 on x-edge i=0
        data[j][0] = (Real)0;

        // data = 0 on x-edge i=imax
        data[j][mIMax] = (Real)0;
    }

    // y-edge-interior
    for (int i = 1; i < mIMax; ++i)
    {
        // data = 0 on y-edge j=0
        data[0][i] = (Real)0;

        // data = 0 on y-edge j=jmax
        data[mJMax][i] = (Real)0;
    }

    // data = 0 at corner (0,0)
    data[0][0] = (Real)0;

    // data = 0 at corner (imax,0)
    data[0][mIMax] = (Real)0;

    // data = 0 at corner (0,jmax)
    data[mJMax][0] = (Real)0;

    // data = 0 at corner (imax,jmax)
    data[mJMax][mIMax] = (Real)0;
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::NeumannBoundaryZero (Real** data)
{
    // x-edge-interior
    for (int j = 1; j < mJMax; ++j)
    {
        // (-1,0)*grad(data) = 0 on x-edge i=0
        data[j][0] = data[j][1];

        // (+1,0)*grad(data) = 0 on x-edge i=imax
        data[j][mIMax] = data[j][mIMaxM1];
    }

    // y-edge-interior
    for (int i = 1; i < mIMax; ++i)
    {
        // (0,-1)*grad(data) = 0 on y-edge j=0
        data[0][i] = data[1][i];

        // (0,+1)*grad(data) = 0 on y-edge j=jmax
        data[mJMax][i] = data[mJMaxM1][i];
    }

    // (-1,-1)*grad(data) = 0 at corner (0,0)
    data[0][0] = data[1][1];

    // (+1,-1)*grad(data) = 0 at corner (imax,0)
    data[0][mIMax] = data[1][mIMaxM1];

    // (-1,+1)*grad(data) = 0 at corner (0,jmax)
    data[mJMax][0] = data[mJMaxM1][1];

    // (+1,+1)*grad(data) = 0 at corner (imax,jmax)
    data[mJMax][mIMax] = data[mJMaxM1][mIMaxM1];
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::UpdateDensityBoundary ()
{
    if (mDensityDirichlet)
    {
        DirichletBoundaryZero(mDensity1);
    }
    else
    {
        NeumannBoundaryZero(mDensity1);
    }
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::UpdateVelocityBoundary ()
{
    // The velocity is (u(x,y), v(x,y)).

    // x-edge-interior
    for (int j = 1; j < mJMax; ++j)
    {
        // u = 0, (-1,0)*grad(v) = 0 on x-edge i=0
        mVelocity1[j][0] = Vector2<Real>(
            (Real)0,
            mVelocity1[j][1][1]);

        // u = 0, (+1,0)*grad(v) = 0 on x-edge i=imax
        mVelocity1[j][mIMax] = Vector2<Real>(
            (Real)0,
            mVelocity1[j][mIMaxM1][1]);
    }

    // y-edge-interior
    for (int i = 1; i < mIMax; ++i)
    {
        // (0,-1)*grad(u) = 0, v = 0 on y-edge j=0
        mVelocity1[0][i] = Vector2<Real>(
            mVelocity1[1][i][0],
            (Real)0);

        // (0,+1)*grad(u) = 0, v = 0 on y-edge j=jmax
        mVelocity1[mJMax][i] = Vector2<Real>(
            mVelocity1[mJMaxM1][i][0],
            (Real)0);
    }

    // (u,v) = (0,0) at corner (0,0)
    mVelocity1[0][0] = Vector2<Real>::ZERO;

    // (u,v) = (0,0) at corner (imax,0)
    mVelocity1[0][mIMax] = Vector2<Real>::ZERO;

    // (u,v) = (0,0) at corner (0,jmax)
    mVelocity1[mJMax][0] = Vector2<Real>::ZERO;

    // (u,v) = (0,0) at corner (imax,jmax)
    mVelocity1[mJMax][mIMax] = Vector2<Real>::ZERO;
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::SwapDensityBuffers ()
{
    Real** save = mDensity0;
    mDensity0 = mDensity1;
    mDensity1 = save;
}
//----------------------------------------------------------------------------
template <typename Real>
void Fluid2Da<Real>::SwapVelocityBuffers ()
{
    Vector2<Real>** save = mVelocity0;
    mVelocity0 = mVelocity1;
    mVelocity1 = save;
}
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Explicit instantiation.
//----------------------------------------------------------------------------
template WM5_PHYSICS_ITEM
class Fluid2Da<float>;

template WM5_PHYSICS_ITEM
class Fluid2Da<double>;
//----------------------------------------------------------------------------
}
