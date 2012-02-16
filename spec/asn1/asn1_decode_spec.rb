require 'rspec'
require 'krypt-core'
require 'stringio'

describe Krypt::ASN1 do 
  describe '#decode' do
    cert = <<__EOC__
-----BEGIN CERTIFICATE-----
MIIC8zCCAdugAwIBAgIBATANBgkqhkiG9w0BAQUFADA9MRMwEQYKCZImiZPyLGQB
GRYDb3JnMRkwFwYKCZImiZPyLGQBGRYJcnVieS1sYW5nMQswCQYDVQQDDAJDQTAe
Fw0xMTA5MjUxMzQ4MjZaFw0xMTA5MjUxNDQ4MjZaMD0xEzARBgoJkiaJk/IsZAEZ
FgNvcmcxGTAXBgoJkiaJk/IsZAEZFglydWJ5LWxhbmcxCzAJBgNVBAMMAkNBMIIB
IjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuV9ht9J7k4NBs38jOXvvTKY9
gW8nLICSno5EETR1cuF7i4pNs9I1QJGAFAX0BEO4KbzXmuOvfCpD3CU+Slp1enen
fzq/t/e/1IRW0wkJUJUFQign4CtrkJL+P07yx18UjyPlBXb81ApEmAB5mrJVSrWm
qbjs07JbuS4QQGGXLc+Su96DkYKmSNVjBiLxVVSpyZfAY3hD37d60uG+X8xdW5v6
8JkRFIhdGlb6JL8fllf/A/blNwdJOhVr9mESHhwGjwfSeTDPfd8ZLE027E5lyAVX
9KZYcU00mOX+fdxOSnGqS/8JDRh0EPHDL15RcJjV2J6vZjPb0rOYGDoMcH+94wID
AQABMA0GCSqGSIb3DQEBBQUAA4IBAQAiAtrIr1pLX4GYN5klviWKb8HC9ICYuAFI
NfE3FwqzErEVXotuMe3yPVyB3Bv6rjYY/x5EtS5+WPTbHlvHZTkfcsnTpizcn4mW
dJ6dDRaFCHt1YKKjUxqBt9lvvrc3nReYZN/P+s1mrDhWzGf8iPZgf8sFUHgnaK7W
CXRVXmPFgCDRNpDDVQ0MQkr509yYfTH+dujNzqTCwSvkyZFyQ7Oe8Yj0VR6kquG3
rEzBQ0F9dUyqQ9gyRg8KHhDfv9HzT1d/rnUZMkoombwYBRIUChGCYV0GnJcan2Zm
/93PnPG1IvPjYNd5VlV+sXSnaxQn974HRCsMv7jA8BD6IgSaX6WK
-----END CERTIFICATE-----
__EOC__
    it "should fail gracefully for unknown tag number (13)" do
      -> { Krypt::ASN1.decode(cert) }.should raise_error Krypt::ASN1::ASN1Error
    end

    it "should handle IO as an IO" do
      io = StringIO.new(
        [
          Krypt::ASN1::Null.new,
          Krypt::ASN1::Integer.new(0)
        ].map { |e| e.to_der }.join
      )
      Krypt::ASN1.decode(io).should be_instance_of Krypt::ASN1::Null
      Krypt::ASN1.decode(io).should be_instance_of Krypt::ASN1::Integer
    end

    it "should parse indefinite length constructive" do
      raw = "\x30\x80\x02\x01\x01\x80\x01\x02\x00\x00"
      Krypt::ASN1.decode(raw).value.size.should == 3
    end
  end
end