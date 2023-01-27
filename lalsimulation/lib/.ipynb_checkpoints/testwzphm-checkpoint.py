import numpy as np
import bilby

np.random.seed(1)

duration = 12
sampling_frequency = 2048.

outdir = 'waveformGen_outdir'
label = 'waveformGen'
bilby.core.utils.check_directory_exists_and_if_not_mkdir(outdir)

injection_parameters = dict(
    mass_1=2.6,
    mass_2=23.0,
    a_1=0.4,
    a_2=0.3,
    tilt_1=0.5,
    tilt_2=1.0,
    phi_12=1.7,
    phi_jl=0.3,
    luminosity_distance=1000.0,
    theta_jn=0.4,
    psi=2.659,
    phase=1.3,
    geocent_time=1126259642.413,
    ra=1.375,
    dec=-1.2108,
)

# Fixed arguments passed into the source model
waveform_arguments = dict(
    waveform_approximant="IMRPhenomZPHM",
    reference_frequency=50.0,
    minimum_frequency=40.0,
)
waveform_generator = bilby.gw.WaveformGenerator(
    duration=duration, sampling_frequency=sampling_frequency,
    frequency_domain_source_model=bilby.gw.source.lal_binary_black_hole,
    parameters=injection_parameters, waveform_arguments=waveform_arguments)
hf_signal = waveform_generator.frequency_domain_strain(injection_parameters)

H1 = bilby.gw.detector.get_interferometer_with_fake_noise_and_injection(
    'H1', injection_polarizations=hf_signal,
    injection_parameters=injection_parameters, duration=duration,
    sampling_frequency=sampling_frequency, outdir=outdir, zero_noise=True)  #zero_noise bool sets noise to 0 if True, leave out to add noise in injection

t0 = injection_parameters['geocent_time']
H1.plot_time_domain_data(outdir=outdir, label=label, notches=[50],
                         bandpass_frequencies=(50, 200), start_end=(-0.5, 0.5),
                         t0=t0)