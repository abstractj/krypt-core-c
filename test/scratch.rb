# encoding: UTF-8

require 'krypt-core'
require_relative 'resources'
require 'stringio'
require 'pp'
require 'openssl'

def s(string)
  Krypt::ASN1::OctetString.new(string)
end

def i(int)
  Krypt::ASN1::Integer.new(int)
end

def eoc
  Krypt::ASN1::EndOfContents.new
end

asn1 = Krypt::ASN1::UTF8String.new '$B$3$s$K$A$O!"@$3&!*(B'
pp asn1
pp asn1.to_der

