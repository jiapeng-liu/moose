//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

// MOOSE includes
#include "SetupPostprocessorDataAction.h"
#include "FEProblem.h"

registerMooseAction("MooseApp", SetupPostprocessorDataAction, "setup_postprocessor_data");

defineLegacyParams(SetupPostprocessorDataAction);

InputParameters
SetupPostprocessorDataAction::validParams()
{
  InputParameters params = Action::validParams();
  params.addClassDescription("Method for initializing the storage for Postprocessors.");
  return params;
}

SetupPostprocessorDataAction::SetupPostprocessorDataAction(InputParameters params) : Action(params)
{
}

void
SetupPostprocessorDataAction::act()
{
  _problem->initPostprocessorData(_name);
}
