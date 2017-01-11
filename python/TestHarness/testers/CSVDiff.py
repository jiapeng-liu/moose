from RunApp import RunApp
from CSVDiffer import CSVDiffer
import os

class CSVDiff(RunApp):

  @staticmethod
  def validParams():
    params = RunApp.validParams()
    params.addRequiredParam('csvdiff',   [], "A list of files to run CSVDiff on.")
    params.addParam('gold_dir',      'gold', "The directory where the \"golden standard\" files reside relative to the TEST_DIR: (default: ./gold/)")
    params.addParam('abs_zero',       1e-10, "Absolute zero cutoff used in exodiff comparisons.")
    params.addParam('rel_err',       5.5e-6, "Relative error value used in exodiff comparisons.")
    params.addParam('delete_output_before_running',  True, "Delete pre-existing output files before running test. Only set to False if you know what you're doing!")

    return params

  def __init__(self, name, params):
    RunApp.__init__(self, name, params)

  def prepare(self):
    if self.specs['delete_output_before_running'] == True:
      self.deleteFilesAndFolders(self.specs['test_dir'], self.specs['csvdiff'])

  def processResults(self, moose_dir, retcode, options, output):
    output = RunApp.processResults(self, moose_dir, retcode, options, output)

    specs = self.specs
    if self.getStatus() == 'FAIL' or specs['skip_checks']:
      return output

    # Don't Run CSVDiff on Scaled Tests
    if options.scaling and specs['scale_refine']:
      return output

    if len(specs['csvdiff']) > 0:
      differ = CSVDiffer( specs['test_dir'], specs['csvdiff'], specs['abs_zero'], specs['rel_err'] )
      msg = differ.diff()
      output += 'Running CSVDiffer.py\n' + msg
      if msg != '':
        self.setStatus('CSVDIFF', 'DIFF')

    return output
