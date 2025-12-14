#include <blib/graphics/transformable.h>
#include <blib/math/utilfuncs.h>



const blib::graphics::TransformMatrix& blib::graphics::ITransformable::getTransform() const
{
    if (this->matrixNeedUpdate)
    {
        //float angle = -m_rotation.z * 3.141592654f / 180.f;
        //float cosine = std::cos(angle);
        //float sine = std::sin(angle);
        //float sxc = m_scale.x * cosine;
        //float syc = m_scale.y * cosine;
        //float sxs = m_scale.x * sine;
        //float sys = m_scale.y * sine;
        //float tx = -m_origin.x * sxc - m_origin.y * sys + m_position.x;
        //float ty = m_origin.x * sxs - m_origin.y * syc + m_position.y;
        //
        //m_transform = Transform(sxc,  sys, tx,
        //                        -sxs, syc, ty,
        //                        0.f,  0.f, 1.f);
        //m_transformNeedUpdate = false;

        TransformMatrix xrotate = blib::graphics::Identity;
        TransformMatrix yrotate = blib::graphics::Identity;
        TransformMatrix zrotate = blib::graphics::Identity;

        TransformMatrix translate;
        //translate.m_matrix[12] = this->m_position.x;
        //translate.m_matrix[13] = this->m_position.y;
        //translate.m_matrix[14] = this->m_position.z;

        translate.data[0][3] = this->transformData.getPosition().x;
        translate.data[1][3] = this->transformData.getPosition().y;
        translate.data[2][3] = this->transformData.getPosition().z;

        if (this->transformData.getRotation().x != 0)
        {
            TransformMatrix dfwerotate;
        }
        //xrotate.rotateX(this->m_rotation.x);
        //yrotate.rotateY(this->m_rotation.y);
        //zrotate.rotateZ(this->m_rotation.z);

        xrotate = blib::graphics::rotateX(xrotate, this->transformData.getRotation().x);
        yrotate = blib::graphics::rotateY(yrotate, this->transformData.getRotation().y);
        zrotate = blib::graphics::rotateZ(zrotate, this->transformData.getRotation().z);

        this->transformMatrix = xrotate * yrotate * zrotate * translate;
        //this->m_transform.m_matrix[12] = this->m_position.x;
        //this->m_transform.m_matrix[13] = this->m_position.y;
        //this->m_transform.m_matrix[14] = this->m_position.z;

        this->matrixNeedUpdate = false;
    }

    return transformMatrix;
}

void blib::graphics::ITransformable::setTransform(const TransformMatrix& transform)
{
    this->transformMatrix = transform;
    blib::graphics::decomposeMatrix(this->transformMatrix, this->transformData);
    this->matrixNeedUpdate = false;
}

blib::graphics::Vector3f blib::graphics::ITransformable::transform(const Vector3f& point) const
{
    blib::math::Matrix<float, 1, 4>temp;
    temp.data[0][0] = point.x;
    temp.data[0][1] = point.x;
    temp.data[0][2] = point.x;
    temp.data[0][3] = 1;

    blib::math::Matrix<float, 1, 4> resMat3 = this->getTransform() * temp;
    
    Vector3f res;
    res.x = resMat3.data[0][0];
    res.y = resMat3.data[0][1];
    res.z = resMat3.data[0][2];
    return res;
}
