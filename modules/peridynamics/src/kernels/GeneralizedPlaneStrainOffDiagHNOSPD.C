//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GeneralizedPlaneStrainOffDiagHNOSPD.h"
#include "MooseVariableScalar.h"
#include "PeridynamicsMesh.h"
#include "RankTwoTensor.h"
#include "RankFourTensor.h"

registerMooseObject("PeridynamicsApp", GeneralizedPlaneStrainOffDiagHNOSPD);

InputParameters
GeneralizedPlaneStrainOffDiagHNOSPD::validParams()
{
  InputParameters params = MechanicsBaseNOSPD::validParams();
  params.addClassDescription(
      "Class for calculating the off-diagonal Jacobian corresponding to "
      "coupling between displacements (or temperature) with scalar out-of-plane strain for "
      "the generalized plane strain using the HNOSPD formulation");

  params.addCoupledVar("scalar_out_of_plane_strain",
                       "Scalar variable for strain in the out-of-plane direction");

  return params;
}

GeneralizedPlaneStrainOffDiagHNOSPD::GeneralizedPlaneStrainOffDiagHNOSPD(
    const InputParameters & parameters)
  : MechanicsBaseNOSPD(parameters),
    _scalar_out_of_plane_strain_var_num(coupledScalar("scalar_out_of_plane_strain"))
{
  // Consistency check
  if (_disp_var.size() != 2)
    mooseError("GeneralizedPlaneStrain only works for two dimensional models!");
}

void
GeneralizedPlaneStrainOffDiagHNOSPD::computeOffDiagJacobianScalar(unsigned int jvar_num)
{
  if (jvar_num == _scalar_out_of_plane_strain_var_num)
  {
    prepare();

    if (_var.number() == _disp_var[0]->number())
      if (_use_full_jacobian)
        computeDispFullOffDiagJacobianScalar(0, jvar_num);
      else
        computeDispPartialOffDiagJacobianScalar(0, jvar_num);
    else if (_var.number() == _disp_var[1]->number())
      if (_use_full_jacobian)
        computeDispFullOffDiagJacobianScalar(1, jvar_num);
      else
        computeDispPartialOffDiagJacobianScalar(1, jvar_num);
    else if (_temp_coupled ? _var.number() == _temp_var->number() : 0)
      computeTempOffDiagJacobianScalar(jvar_num);
  }
}

void
GeneralizedPlaneStrainOffDiagHNOSPD::computeDispPartialOffDiagJacobianScalar(unsigned int component,
                                                                             unsigned int jvar_num)
{
  // off-diagonal jacobian entries on the column and row corresponding to
  // scalar_out_of_plane_strain for coupling with displacements

  DenseMatrix<Number> & ken = _assembly.jacobianBlock(_var.number(), jvar_num);
  DenseMatrix<Number> & kne = _assembly.jacobianBlock(jvar_num, _var.number());
  MooseVariableScalar & jvar = _sys.getScalarVariable(_tid, jvar_num);

  // fill in the column corresponding to the scalar variable
  std::vector<RankTwoTensor> dSdE33(_nnodes);
  for (unsigned int nd = 0; nd < _nnodes; ++nd)
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int j = 0; j < 3; ++j)
        dSdE33[nd](i, j) = _Jacobian_mult[nd](i, j, 2, 2);

  for (_i = 0; _i < _test.size(); ++_i)
    for (_j = 0; _j < jvar.order(); ++_j)
      ken(_i, _j) += (_i == _j ? -1 : 1) *
                     (_multi[0] * (dSdE33[0] * _shape2[0].inverse()).row(component) +
                      _multi[1] * (dSdE33[1] * _shape2[1].inverse()).row(component)) *
                     _origin_vec * _bond_status;

  kne(0, 0) +=
      computeDSDU(component, 0)(2, 2) * _node_vol[0] * _dg_vol_frac[0] * _bond_status; // node i
  kne(0, 1) +=
      computeDSDU(component, 1)(2, 2) * _node_vol[1] * _dg_vol_frac[1] * _bond_status; // node j
}

void
GeneralizedPlaneStrainOffDiagHNOSPD::computeDispFullOffDiagJacobianScalar(unsigned int component,
                                                                          unsigned int jvar_num)
{
  // LOCAL contribution

  // off-diagonal jacobian entries on the column and row corresponding to
  // scalar_out_of_plane_strain for coupling with displacements
  DenseMatrix<Number> & ken = _assembly.jacobianBlock(_var.number(), jvar_num);
  DenseMatrix<Number> & kne = _assembly.jacobianBlock(jvar_num, _var.number());
  MooseVariableScalar & jvar = _sys.getScalarVariable(_tid, jvar_num);

  // fill in the column corresponding to the scalar variable
  std::vector<RankTwoTensor> dSdE33(_nnodes);
  for (unsigned int nd = 0; nd < _nnodes; ++nd)
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int j = 0; j < 3; ++j)
        dSdE33[nd](i, j) = _Jacobian_mult[nd](i, j, 2, 2);

  for (_i = 0; _i < _test.size(); ++_i)
    for (_j = 0; _j < jvar.order(); ++_j)
      ken(_i, _j) += (_i == _j ? -1 : 1) *
                     (_multi[0] * (dSdE33[0] * _shape2[0].inverse()).row(component) +
                      _multi[1] * (dSdE33[1] * _shape2[1].inverse()).row(component)) *
                     _origin_vec * _bond_status;

  // fill in the row corresponding to the scalar variable
  kne(0, 0) +=
      computeDSDU(component, 0)(2, 2) * _node_vol[0] * _dg_vol_frac[0] * _bond_status; // node i
  kne(0, 1) +=
      computeDSDU(component, 1)(2, 2) * _node_vol[1] * _dg_vol_frac[1] * _bond_status; // node j

  // NONLOCAL contribution

  // fill in the row corresponding to the scalar variable
  for (unsigned int nd = 0; nd < _nnodes; ++nd)
  {
    // calculation of jacobian contribution to current_node's neighbors
    // NOT including the contribution to nodes i and j, which is considered as local off-diagonal
    std::vector<dof_id_type> ivardofs(_nnodes);
    ivardofs[0] = _current_elem->node_ptr(nd)->dof_number(_sys.number(), _var.number(), 0);
    std::vector<dof_id_type> neighbors = _pdmesh.getNeighbors(_current_elem->node_id(nd));
    unsigned int nb_index =
        std::find(neighbors.begin(), neighbors.end(), _current_elem->node_id(1 - nd)) -
        neighbors.begin();
    std::vector<dof_id_type> dg_neighbors =
        _pdmesh.getDefGradNeighbors(_current_elem->node_id(nd), nb_index);
    std::vector<dof_id_type> bonds = _pdmesh.getBonds(_current_elem->node_id(nd));

    Real vol_nb;
    RealGradient origin_vec_nb;
    RankTwoTensor dFdUk, dPdUk;

    for (unsigned int nb = 0; nb < dg_neighbors.size(); ++nb)
      if (_bond_status_var->getElementalValue(_pdmesh.elemPtr(bonds[dg_neighbors[nb]])) > 0.5)
      {
        Node * dgneighbor_nb = _pdmesh.nodePtr(neighbors[dg_neighbors[nb]]);
        ivardofs[1] = dgneighbor_nb->dof_number(_sys.number(), _var.number(), 0);
        vol_nb = _pdmesh.getPDNodeVolume(neighbors[dg_neighbors[nb]]);

        // obtain bond nb's origin vector
        origin_vec_nb = *dgneighbor_nb - *_pdmesh.nodePtr(_current_elem->node_id(nd));

        dFdUk.zero();
        for (unsigned int k = 0; k < _dim; ++k)
          dFdUk(component, k) = _horiz_rad[nd] / origin_vec_nb.norm() * origin_vec_nb(k) * vol_nb;

        dFdUk *= _shape2[nd].inverse();
        dPdUk = _Jacobian_mult[nd] * 0.5 * (dFdUk.transpose() + dFdUk);

        _local_ke.resize(ken.n(), ken.m());
        _local_ke.zero();
        _local_ke(0, 1) = dPdUk(2, 2) * _dg_vol_frac[nd] * _node_vol[nd] * _bond_status;

        _assembly.cacheJacobianBlock(_local_ke, jvar.dofIndices(), ivardofs, _var.scalingFactor());
      }
  }
}

void
GeneralizedPlaneStrainOffDiagHNOSPD::computeTempOffDiagJacobianScalar(unsigned int jvar_num)
{
  // off-diagonal jacobian entries on the row corresponding to scalar_out_of_plane_strain for
  // coupling with temperature
  DenseMatrix<Number> & kne = _assembly.jacobianBlock(jvar_num, _var.number());

  // one-way coupling between the scalar_out_of_plane_strain and temperature. fill in the row
  // corresponding to the scalar_out_of_plane_strain
  std::vector<RankTwoTensor> dSdT(_nnodes);
  for (unsigned int nd = 0; nd < _nnodes; ++nd)
    for (unsigned int es = 0; es < _deigenstrain_dT.size(); ++es)
      dSdT[nd] = -_Jacobian_mult[nd] * (*_deigenstrain_dT[es])[nd];

  kne(0, 0) += dSdT[0](2, 2) * _dg_vol_frac[0] * _node_vol[0] * _bond_status; // node i
  kne(0, 1) += dSdT[1](2, 2) * _dg_vol_frac[1] * _node_vol[1] * _bond_status; // node j
}
