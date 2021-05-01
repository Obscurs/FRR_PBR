// Author: Marc Comino 2020

#include <triangle_mesh.h>

#include <algorithm>
#include <limits>

namespace data_representation {

TriangleMesh::TriangleMesh() { Clear(); }

void TriangleMesh::Clear() {
  vertices_.clear();
  faces_.clear();
  normals_.clear();
  buffer_.clear();

  min_ = Eigen::Vector3f(std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max());
  max_ = Eigen::Vector3f(std::numeric_limits<float>::lowest(),
                         std::numeric_limits<float>::lowest(),
                         std::numeric_limits<float>::lowest());
}

void TriangleMesh::prepareVertexBuffer()
{
    buffer_.clear();
    for(unsigned int i=0; i < faces_.size();++i)
    {
        for(int j=0; j< 3; ++j)
        {
            buffer_.push_back(vertices_[i*3+j]);
        }
        for(int j=0; j< 3; ++j)
        {
            buffer_.push_back(normals_[i*3+j]);
        }
    }
}
}  // namespace data_representation
