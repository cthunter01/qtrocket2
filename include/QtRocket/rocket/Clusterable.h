#pragma once

#include "QtRocket/rocket/Instanceable.h"

namespace QtRocket
{

class ClusterConfiguration;

/// A component that can be arranged in one of the cluster layouts, such as a clustered inner
/// tube (OpenRocket's Clusterable). ClusterConfiguration (the 15 layouts) belongs to the rocket
/// components group and is only declared here; its layouts are immutable shared constants, so
/// they are passed by reference.
///
/// Java's Clusterable also extends ChangeSource; that part is RocketComponent's
/// addChangeListener() here, so it is not repeated.
class Clusterable : public virtual Instanceable
{
public:
    ~Clusterable() override = default;

    Clusterable& operator=(const Clusterable&) = delete;
    Clusterable& operator=(Clusterable&&)      = delete;

    [[nodiscard]] virtual const ClusterConfiguration& getClusterConfiguration() const = 0;
    virtual void setClusterConfiguration(const ClusterConfiguration& cluster)         = 0;

    /// The distance between the centers of neighbouring cluster members.
    [[nodiscard]] virtual double getClusterSeparation() const = 0;

protected:
    Clusterable()                   = default;
    Clusterable(const Clusterable&) = default;
    Clusterable(Clusterable&&)      = default;
};

}  // namespace QtRocket
