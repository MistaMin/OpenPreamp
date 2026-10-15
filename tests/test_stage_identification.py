"""Contracts that prevent misleading offline identification results."""
import csv,sys,tempfile,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from fit_measured_stages import aligned
from check_stage_captures import check

class StageIdentification(unittest.TestCase):
    def test_negative_gain_does_not_flip_even_harmonics(self):
        w=2*np.pi*1000*np.arange(9600)/96000;phase=-.17
        x=.7*np.sin(w);delayed=.7*np.sin(w+phase)
        f=lambda u:u+.15*u*u+.03*u*u*u
        _,estimate=aligned(np.column_stack([x,-3*f(delayed)]),phase,-3)
        self.assertLess(np.max(abs(estimate-f(x))),1e-12)

    def test_driven_gain_is_not_renormalized_to_unity(self):
        x=np.sin(2*np.pi*np.arange(9600)/96)
        _,estimate=aligned(np.column_stack([x,x]),0,2)
        self.assertLess(np.max(abs(estimate-.5*x)),1e-12)

    def make_capture(self,path,mask):
        n=len(mask);(path/'fixture.valid').write_bytes(mask)
        columns=['model','fixture','kind','rate','amplitude','full_scale_volts','frames','analysis_begin','valid_frames','longest_valid_run','recovered_steps','hard_failures','accepted']
        valid=sum(mask);run=longest=0
        for flag in mask:run=run+1 if flag else 0;longest=max(longest,run)
        with (path/'fixtures.csv').open('w') as file:
            w=csv.writer(file);w.writerow(columns);w.writerow(['brit','fixture','curve',96000,1,1.7,n,0,valid,longest,0,0,int(valid/n>=.99 and longest>=4608)])
        with (path/'ports.csv').open('w') as file:
            w=csv.writer(file);w.writerow(['model','fixture','stage','input_dc_volts','output_dc_volts'])
            for i in range(5):w.writerow(['brit','fixture','input_coupling' if i==0 else f'stage{i}',0,0])
        for suffix in ['complete','input_coupling']+[f'stage{i}' for i in range(1,5)]:np.zeros((n,2),dtype='<f8').tofile(path/f'fixture-{suffix}.bin')

    def test_stale_source_voltage_rejects_dc_reference(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);self.make_capture(path,bytes([1])*4800)
            ports=(path/'ports.csv').read_text().replace('input_coupling,0,0','input_coupling,7.17,0');(path/'ports.csv').write_text(ports)
            with self.assertRaisesRegex(ValueError,'Source was not reset'):check(path)

    def test_excessive_exclusion_rejects_capture(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);self.make_capture(path,bytes(50)+bytes([1])*4750)
            report=check(path)
            self.assertFalse(report['fixtures'][0]['accepted'])
            self.assertIn('Rejected numerical reference: fixture',report['errors'])

    def test_missing_models_are_not_silently_accepted(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);self.make_capture(path,bytes([1])*4800)
            report=check(path)
            self.assertTrue(report['fixtures'][0]['accepted']);self.assertFalse(report['passed'])

    def test_truncated_stage_file_fails_closed(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);self.make_capture(path,bytes([1])*4800);(path/'fixture-stage1.bin').write_bytes(b'partial')
            with self.assertRaisesRegex(ValueError,'Incomplete sample data'):check(path)

    def test_nonfinite_sample_cannot_be_marked_valid(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);self.make_capture(path,bytes([1])*4800)
            samples=np.zeros((4800,2),dtype='<f8');samples[7,1]=np.nan;samples.tofile(path/'fixture-stage1.bin')
            with self.assertRaisesRegex(ValueError,'nonfinite sample'):check(path)
if __name__=='__main__':unittest.main()
