fr = matlab.io.datastore.DsFileReader('clap.wav');
[d, count] = read(fr, 7904*2+44);
samples = zeros([length(d)/2-44, 1]);
for i=1:length(samples)
    didx1 = 44+i*2-1;
    didx2 = 44+i*2;
    b1 = double(d(didx1));
    b2 = double(d(didx2))*2.0^8;
    sample = b1+b2;
    if (sample > 2^15) 
        sample = sample-2^16;
    end
    samples(i) = sample; 
end