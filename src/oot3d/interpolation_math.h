// Camera interpolation math adapted from the MM3D presentation implementation.
// Copyright 2026 MM3D presentation contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
namespace OOT3D::Interpolation::Math {
using Matrix = std::array<std::array<double,4>,4>;
using Quaternion = std::array<double,4>;
inline Matrix Identity() { Matrix m{}; for(unsigned i=0;i<4;++i)m[i][i]=1; return m; }
inline Matrix Multiply(const Matrix& a,const Matrix& b) {
    Matrix r{};
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)for(unsigned k=0;k<4;++k)r[i][j]+=a[i][k]*b[k][j];
    return r;
}
inline bool Near(const Matrix& a,const Matrix& b,double epsilon) {
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)
        if(!std::isfinite(a[i][j])||!std::isfinite(b[i][j])||std::abs(a[i][j]-b[i][j])>epsilon)return false;
    return true;
}
inline bool Rigid(const Matrix& m) {
    for(const auto& row:m)for(double v:row)if(!std::isfinite(v))return false;
    for(unsigned i=0;i<4;++i)if(std::abs(m[3][i]-(i==3?1.:0.))>1e-6)return false;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j) {
        double dot=0;for(unsigned k=0;k<3;++k)dot+=m[k][i]*m[k][j];
        if(std::abs(dot-(i==j?1.:0.))>.002)return false;
    }
    const double determinant=m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])-
        m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])+m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);
    return std::abs(determinant-1)<.003;
}
inline Matrix InverseRigid(const Matrix& m) {
    Matrix r=Identity();
    for(unsigned i=0;i<3;++i) {
        for(unsigned j=0;j<3;++j)r[i][j]=m[j][i];
        for(unsigned j=0;j<3;++j)r[i][3]-=r[i][j]*m[j][3];
    }
    return r;
}
// Native view matrices have small basis errors from the game's camera math.
// Transposing such a basis is not its exact inverse: at large scene coordinates
// it introduces a visible translation even for a stationary model. Validate the
// camera shape, then invert its affine matrix so capture/recomposition cancels.
inline std::optional<Matrix> InverseCamera(const Matrix& m) {
    if(!Rigid(m))return {};
    const auto a=m[0][0],b=m[0][1],c=m[0][2],d=m[1][0],e=m[1][1],f=m[1][2],g=m[2][0],h=m[2][1],i=m[2][2];
    const auto det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(det) || std::abs(det)<1e-12)return {};
    auto r=Identity();
    r[0]={(e*i-f*h)/det,(c*h-b*i)/det,(b*f-c*e)/det,0};
    r[1]={(f*g-d*i)/det,(a*i-c*g)/det,(c*d-a*f)/det,0};
    r[2]={(d*h-e*g)/det,(b*g-a*h)/det,(a*e-b*d)/det,0};
    for(unsigned row=0;row<3;++row)for(unsigned k=0;k<3;++k)r[row][3]-=r[row][k]*m[k][3];
    return r;
}
inline Quaternion Rotation(const Matrix& m) {
    Quaternion q{};
    const double trace=m[0][0]+m[1][1]+m[2][2];
    if(trace>0) {
        const double s=2*std::sqrt(trace+1);
        q={s/4,(m[2][1]-m[1][2])/s,(m[0][2]-m[2][0])/s,(m[1][0]-m[0][1])/s};
    } else {
        unsigned i=0;for(unsigned k=1;k<3;++k)if(m[k][k]>m[i][i])i=k;
        const unsigned j=(i+1)%3,k=(i+2)%3;
        const double s=2*std::sqrt(1+m[i][i]-m[j][j]-m[k][k]);
        q[0]=(m[k][j]-m[j][k])/s; q[i+1]=s/4;
        q[j+1]=(m[j][i]+m[i][j])/s;q[k+1]=(m[k][i]+m[i][k])/s;
    }
    double length=0;for(double v:q)length+=v*v;length=std::sqrt(length);
    for(double& v:q)v/=length;
    return q;
}
inline Matrix Blend(const Matrix& a,const Matrix& b,double phase) {
    phase=std::clamp(phase,0.,1.);
    auto qa=Rotation(a),qb=Rotation(b);
    double dot=0;for(unsigned i=0;i<4;++i)dot+=qa[i]*qb[i];
    Quaternion q{};double length=0;
    for(unsigned i=0;i<4;++i){q[i]=(1-phase)*qa[i]+phase*(dot<0?-qb[i]:qb[i]);length+=q[i]*q[i];}
    length=std::sqrt(length);for(double& v:q)v/=length;
    const auto [w,x,y,z]=q;
    Matrix r=Identity();
    r[0][0]=1-2*(y*y+z*z);r[0][1]=2*(x*y-z*w);r[0][2]=2*(x*z+y*w);
    r[1][0]=2*(x*y+z*w);r[1][1]=1-2*(x*x+z*z);r[1][2]=2*(y*z-x*w);
    r[2][0]=2*(x*z-y*w);r[2][1]=2*(y*z+x*w);r[2][2]=1-2*(x*x+y*y);
    for(unsigned i=0;i<3;++i)r[i][3]=a[i][3]+(b[i][3]-a[i][3])*phase;
    return r;
}
}
