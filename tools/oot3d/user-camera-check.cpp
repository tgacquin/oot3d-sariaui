// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/user_camera_config.h"
#include "oot3d/orbit_camera.h"
#include <QTemporaryDir>
#include <cassert>
#include <iostream>
#include <limits>

int main() {
    namespace U=OOT3D::UserCamera;
    namespace O=OOT3D::OrbitCamera;
    namespace C=OOT3D::Camera;
    const auto near=[](float a,float b){return std::abs(a-b)<.00001f;};
    QTemporaryDir dir;assert(dir.isValid());
    QSettings config(dir.filePath("camera.ini"),QSettings::IniFormat);
    config.setValue("Graphics/field_of_view",64.f);
    config.setValue("Camera/distance_scale",1.2f);
    config.setValue("Camera/sensitivity",1.5f);
    config.setValue("Camera/acceleration_time",.3f);
    config.setValue("Camera/deceleration_time",.4f);
    config.setValue("Camera/invert_x",true);
    config.setValue("Camera/invert_y",false);
    config.setValue("Camera/smooth_collision_return",false);
    const auto authored_keys=config.allKeys();
    QList<QVariant> authored_values;
    for(const auto& key:authored_keys)authored_values.push_back(config.value(key));
    C::desired_fov=64;O::distance_scale=1.2f;O::sensitivity=1.5f;
    O::acceleration_time=.3f;O::deceleration_time=.4f;
    O::invert_x=true;O::invert_y=false;O::smooth_collision_return=false;
    U::LoadConfig(config);
    assert(U::Get()==U::Values{} && config.allKeys()==authored_keys && !U::dirty);
    assert(C::ConfiguredFov()==64 && O::ConfiguredDistance()==1.2f);
    assert(O::ConfiguredSensitivity()==1.5f && O::ConfiguredAcceleration()==.3f);
    assert(O::ConfiguredDeceleration()==.4f && O::ConfiguredInvertX());
    assert(!O::ConfiguredInvertY() && !O::ConfiguredSmoothPullback());

    U::Values player;player.scales={1.1f,1.25f,.5f,2.f,.5f};player.toggles={1,0,1};
    U::Set(player);
    for(int frame=0;frame<100;++frame) {
        assert(near(C::ConfiguredFov(),70.4f) && near(O::ConfiguredDistance(),1.5f));
        assert(near(O::ConfiguredSensitivity(),.75f));
        assert(near(O::ConfiguredAcceleration(),.6f) && near(O::ConfiguredDeceleration(),.2f));
        assert(!O::ConfiguredInvertX() && O::ConfiguredInvertY() && O::ConfiguredSmoothPullback());
        assert(C::desired_fov==64 && O::distance_scale==1.2f && O::sensitivity==1.5f);
        assert(O::acceleration_time==.3f && O::deceleration_time==.4f);
        assert(O::invert_x && !O::invert_y && !O::smooth_collision_return);
    }
    assert(U::dirty && !C::dirty && !O::dirty);
    U::SaveConfig(config);config.sync();
    for(qsizetype i=0;i<authored_keys.size();++i)assert(config.value(authored_keys[i])==authored_values[i]);
    U::Set({},false);
    QSettings reopened(dir.filePath("camera.ini"),QSettings::IniFormat);
    U::LoadConfig(reopened);assert(U::Get()==player);
    U::ResetOrbit(player);U::Set(player);
    assert(player.scales[U::Fov]==1.1f && player.scales[U::Distance]==1.25f);
    assert(O::ConfiguredSensitivity()==1.5f && O::ConfiguredAcceleration()==.3f);
    assert(O::ConfiguredInvertX() && !O::ConfiguredSmoothPullback());
    // Reset restores inheritance, not a snapshot of old defaults.
    O::sensitivity=2;O::acceleration_time=.15f;O::invert_x=false;O::smooth_collision_return=true;
    assert(O::ConfiguredSensitivity()==2 && O::ConfiguredAcceleration()==.15f);
    assert(!O::ConfiguredInvertX() && O::ConfiguredSmoothPullback());
    player.scales[U::Fov]=player.scales[U::Distance]=1;U::Set(player);
    assert(C::ConfiguredFov()==64 && O::ConfiguredDistance()==1.2f);
    U::SaveConfig(config);U::LoadConfig(config);assert(U::Get()==U::Values{});
    // Extreme player settings respect the established camera limits.
    player.scales.fill(4);U::Set(player);
    assert(U::Get().scales[U::Fov]==1.25f && U::Get().scales[U::Distance]==1.25f);
    assert(C::ConfiguredFov()==C::MaximumFov && near(O::ConfiguredDistance(),1.5f));
    assert(O::ConfiguredSensitivity()==3 && O::ConfiguredDeceleration()==1);
    O::acceleration_time=0;assert(O::ConfiguredAcceleration()==0);
    // Old out-of-range F2 overrides clamp on load; F1 remains untouched.
    config.setValue("UserCamera/fov",1.84f);
    config.setValue("UserCamera/distance",.46f);
    U::LoadConfig(config);
    assert(U::Get().scales[U::Fov]==1.25f && U::Get().scales[U::Distance]==.75f);
    assert(near(O::ConfiguredDistance(),.9f));
    for(qsizetype i=0;i<authored_keys.size();++i)assert(config.value(authored_keys[i])==authored_values[i]);
    config.setValue("UserCamera/fov","nan");
    config.setValue("UserCamera/distance","bad");
    config.setValue("UserCamera/sensitivity",100);
    config.setValue("UserCamera/acceleration",-1);
    config.setValue("UserCamera/invert_x",9);
    config.setValue("UserCamera/invert_y","bad");
    U::LoadConfig(config);const auto validated=U::Get();
    assert(validated.scales[U::Fov]==1 && validated.scales[U::Distance]==1);
    assert(validated.scales[U::Sensitivity]==4 && validated.scales[U::Acceleration]==.25f);
    assert(validated.toggles[U::InvertX]==-1 && validated.toggles[U::InvertY]==-1);
    std::cout<<"Camera: relative composition, authored isolation, reset inheritance, persistence and limits passed\n";
}
