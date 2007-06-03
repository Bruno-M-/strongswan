/**
 * @file ac.c
 * 
 * @brief Implementation of x509ac_t.
 * 
 */

/* 
 * Copyright (C) 2002 Ueli Galizzi, Ariane Seiler
 * Copyright (C) 2003 Martin Berner, Lukas Suter
 * Copyright (C) 2007 Andreas Steffen, Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <library.h>
#include <debug.h>

#include <asn1/asn1.h>
#include <utils/identification.h>
#include <utils/linked_list.h>

#include "ac.h"

typedef struct private_x509ac_t private_x509ac_t;

/**
 * Private data of a x509ac_t object.
 */
struct private_x509ac_t {
	/**
	 * Public interface for this attribute certificate.
	 */
	x509ac_t public;

	/**
	 * Time when attribute certificate was installed
	 */
	time_t installed;

	/**
	 * X.509 attribute certificate in DER format
	 */
	chunk_t certificate;

	/**
	 * X.509 attribute certificate body over which signature is computed
	 */
	chunk_t certificateInfo;

	/**
	 * Version of the X.509 attribute certificate
	 */
	u_int version;

	/**
	 * Serial number of the X.509 attribute certificate
	 */
	chunk_t serialNumber;

	/**
	 * ID representing the issuer of the holder certificate
	 */
	identification_t *holderIssuer;

	/**
	 * Serial number of the holder certificate
	 */
	chunk_t holderSerial;

	/**
	 * ID representing the holder
	 */
	identification_t *entityName;
	
	/**
	 * ID representing the attribute certificate issuer
	 */
	identification_t *issuerName;

	/**
	 * Signature algorithm
	 */
	int sigAlg;

	/**
	 * Start time of certificate validity
	 */
	time_t notBefore;

	/**
	 * End time of certificate validity
	 */
	time_t notAfter;

	/**
	 * List of charging attributes
	 */
	linked_list_t *charging;

	/**
	 * List of groub attributes
	 */
	linked_list_t *groups;

	/**
	 * Authority Key Identifier
	 */
	chunk_t authKeyID;

	/**
	 * Authority Key Serial Number
	 */
	chunk_t authKeySerialNumber;

	/**
	 * No revocation information available
	 */
	bool noRevAvail;

	/**
	 * Signature algorithm (must be identical to sigAlg)
	 */
	int algorithm;

	/**
	 * Signature
	 */
	chunk_t signature;
};

/**
 * definition of ietfAttribute kinds
 */
typedef enum {
	IETF_ATTRIBUTE_OCTETS =	0,
	IETF_ATTRIBUTE_OID =	1,
	IETF_ATTRIBUTE_STRING =	2
} ietfAttribute_t;

/**
 * access structure for an ietfAttribute
 */
typedef struct ietfAttr_t ietfAttr_t;

struct ietfAttr_t {
	/**
	 * IETF attribute kind
	 */
	ietfAttribute_t kind;

	/**
	 * IETF attribute valuse
	 */
	chunk_t value;

	/**
	 * Destroys the ietfAttr_t object.
	 * 
	 * @param this			ietfAttr_t to destroy
	 */
	void (*destroy) (ietfAttr_t *this);
};

/**
 * Destroys an ietfAttr_t object
 */
static void ietfAttr_destroy(ietfAttr_t *this)
{
	free(this->value.ptr);
	free(this);
}

/**
 * Creates an ietfAttr_t object.
 */
ietfAttr_t *ietfAttr_create(ietfAttribute_t kind, chunk_t value)
{
	ietfAttr_t *this = malloc_thing(ietfAttr_t);

	/* initialize */
	this->kind = kind;
	this->value = chunk_clone(value);

	/* function */
	this->destroy = ietfAttr_destroy;
	
	return this;
}

/**
 * ASN.1 definition of ietfAttrSyntax
 */
static const asn1Object_t ietfAttrSyntaxObjects[] =
{
	{ 0, "ietfAttrSyntax",		ASN1_SEQUENCE,		ASN1_NONE }, /*  0 */
	{ 1,   "policyAuthority",	ASN1_CONTEXT_C_0,	ASN1_OPT |
													ASN1_BODY }, /*  1 */
	{ 1,   "end opt",			ASN1_EOC,			ASN1_END  }, /*  2 */
	{ 1,   "values",			ASN1_SEQUENCE,		ASN1_LOOP }, /*  3 */
	{ 2,     "octets",			ASN1_OCTET_STRING,	ASN1_OPT |
													ASN1_BODY }, /*  4 */
	{ 2,     "end choice",		ASN1_EOC,			ASN1_END  }, /*  5 */
	{ 2,     "oid",				ASN1_OID,			ASN1_OPT |
													ASN1_BODY }, /*  6 */
	{ 2,     "end choice",		ASN1_EOC,			ASN1_END  }, /*  7 */
	{ 2,     "string",			ASN1_UTF8STRING,	ASN1_OPT |
													ASN1_BODY }, /*  8 */
	{ 2,     "end choice",		ASN1_EOC,			ASN1_END  }, /*  9 */
	{ 1,   "end loop",			ASN1_EOC,			ASN1_END  }  /* 10 */
};

#define IETF_ATTR_OCTETS	 4
#define IETF_ATTR_OID		 6
#define IETF_ATTR_STRING	 8
#define IETF_ATTR_ROOF		11

/**
 * ASN.1 definition of roleSyntax
 */
static const asn1Object_t roleSyntaxObjects[] =
{
	{ 0, "roleSyntax",			ASN1_SEQUENCE,		ASN1_NONE }, /*  0 */
	{ 1,   "roleAuthority",		ASN1_CONTEXT_C_0,	ASN1_OPT |
													ASN1_OBJ  }, /*  1 */
	{ 1,   "end opt",			ASN1_EOC,			ASN1_END  }, /*  2 */
	{ 1,   "roleName",			ASN1_CONTEXT_C_1,	ASN1_OBJ  }  /*  3 */
};

#define ROLE_ROOF		4

/**
 * ASN.1 definition of an X509 attribute certificate
 */
static const asn1Object_t acObjects[] =
{
	{ 0, "AttributeCertificate",			ASN1_SEQUENCE,		  ASN1_OBJ  }, /*  0 */
	{ 1,   "AttributeCertificateInfo",		ASN1_SEQUENCE,		  ASN1_OBJ  }, /*  1 */
	{ 2,	   "version",					ASN1_INTEGER,		  ASN1_DEF |
																  ASN1_BODY }, /*  2 */
	{ 2,	   "holder",					ASN1_SEQUENCE,		  ASN1_NONE }, /*  3 */
	{ 3,	     "baseCertificateID",		ASN1_CONTEXT_C_0,	  ASN1_OPT  }, /*  4 */
	{ 4,	       "issuer",				ASN1_SEQUENCE,		  ASN1_OBJ  }, /*  5 */
	{ 4,	       "serial",				ASN1_INTEGER,		  ASN1_BODY }, /*  6 */
	{ 4,         "issuerUID",				ASN1_BIT_STRING,	  ASN1_OPT |
																  ASN1_BODY }, /*  7 */
	{ 4,         "end opt",					ASN1_EOC,			  ASN1_END  }, /*  8 */
	{ 3,       "end opt",					ASN1_EOC,			  ASN1_END  }, /*  9 */
	{ 3,	     "entityName",				ASN1_CONTEXT_C_1,	  ASN1_OPT |
																  ASN1_OBJ  }, /* 10 */
	{ 3,       "end opt",					ASN1_EOC,			  ASN1_END  }, /* 11 */
	{ 3,	     "objectDigestInfo",		ASN1_CONTEXT_C_2,	  ASN1_OPT  }, /* 12 */
	{ 4,	       "digestedObjectType",	ASN1_ENUMERATED,	  ASN1_BODY }, /* 13*/
	{ 4,	       "otherObjectTypeID",		ASN1_OID,			  ASN1_OPT |
																  ASN1_BODY }, /* 14 */
	{ 4,         "end opt",					ASN1_EOC,			  ASN1_END  }, /* 15*/
	{ 4,         "digestAlgorithm",			ASN1_EOC,			  ASN1_RAW  }, /* 16 */
	{ 3,       "end opt",					ASN1_EOC,			  ASN1_END  }, /* 17 */
	{ 2,	   "v2Form",					ASN1_CONTEXT_C_0,	  ASN1_NONE }, /* 18 */
	{ 3,	     "issuerName",				ASN1_SEQUENCE,		  ASN1_OPT |
																  ASN1_OBJ  }, /* 19 */
	{ 3,       "end opt",					ASN1_EOC,			  ASN1_END  }, /* 20 */
	{ 3,	     "baseCertificateID",		ASN1_CONTEXT_C_0,	  ASN1_OPT  }, /* 21 */
	{ 4,	       "issuerSerial",			ASN1_SEQUENCE,		  ASN1_NONE }, /* 22 */
	{ 5,	         "issuer",				ASN1_SEQUENCE,		  ASN1_OBJ  }, /* 23 */
	{ 5,	  	 "serial",					ASN1_INTEGER,		  ASN1_BODY }, /* 24 */
	{ 5,           "issuerUID",				ASN1_BIT_STRING,	  ASN1_OPT |
																  ASN1_BODY }, /* 25 */
	{ 5,           "end opt",				ASN1_EOC,			  ASN1_END  }, /* 26 */
	{ 3,       "end opt",					ASN1_EOC,			  ASN1_END  }, /* 27 */
	{ 3,       "objectDigestInfo",			ASN1_CONTEXT_C_1,	  ASN1_OPT  }, /* 28 */
	{ 4,	       "digestInfo",			ASN1_SEQUENCE,		  ASN1_OBJ  }, /* 29 */
	{ 5,  	 "digestedObjectType",			ASN1_ENUMERATED,	  ASN1_BODY }, /* 30 */
	{ 5,	  	 "otherObjectTypeID",		ASN1_OID,			  ASN1_OPT |
																  ASN1_BODY }, /* 31 */
	{ 5,           "end opt",				ASN1_EOC,			  ASN1_END  }, /* 32 */
	{ 5,           "digestAlgorithm",		ASN1_EOC,			  ASN1_RAW  }, /* 33 */
	{ 3,       "end opt",					ASN1_EOC,			  ASN1_END  }, /* 34 */
	{ 2,	   "signature",					ASN1_EOC,			  ASN1_RAW  }, /* 35 */
	{ 2,	   "serialNumber",				ASN1_INTEGER,		  ASN1_BODY }, /* 36 */
	{ 2,	   "attrCertValidityPeriod",	ASN1_SEQUENCE,		  ASN1_NONE }, /* 37 */
	{ 3,	     "notBeforeTime",			ASN1_GENERALIZEDTIME, ASN1_BODY }, /* 38 */
	{ 3,	     "notAfterTime",			ASN1_GENERALIZEDTIME, ASN1_BODY }, /* 39 */
	{ 2,	   "attributes",				ASN1_SEQUENCE,		  ASN1_LOOP }, /* 40 */
	{ 3,       "attribute",					ASN1_SEQUENCE,		  ASN1_NONE }, /* 41 */
	{ 4,         "type",					ASN1_OID,			  ASN1_BODY }, /* 42 */
	{ 4,         "values",					ASN1_SET, 			  ASN1_LOOP }, /* 43 */
	{ 5,           "value",					ASN1_EOC, 			  ASN1_RAW  }, /* 44 */
	{ 4, 	       "end loop",				ASN1_EOC,			  ASN1_END  }, /* 45 */
	{ 2,     "end loop",					ASN1_EOC,			  ASN1_END  }, /* 46 */
	{ 2,     "extensions",					ASN1_SEQUENCE,		  ASN1_LOOP }, /* 47 */
	{ 3,       "extension",					ASN1_SEQUENCE,		  ASN1_NONE }, /* 48 */
	{ 4,         "extnID",					ASN1_OID,			  ASN1_BODY }, /* 49 */
	{ 4,         "critical",				ASN1_BOOLEAN,		  ASN1_DEF |
																  ASN1_BODY }, /* 50 */
	{ 4,         "extnValue",				ASN1_OCTET_STRING,	  ASN1_BODY }, /* 51 */
	{ 2,     "end loop",					ASN1_EOC,			  ASN1_END  }, /* 52 */
	{ 1,   "signatureAlgorithm",			ASN1_EOC,			  ASN1_RAW  }, /* 53 */
	{ 1,   "signatureValue",				ASN1_BIT_STRING,	  ASN1_BODY }  /* 54 */
};

#define AC_OBJ_CERTIFICATE			 0
#define AC_OBJ_CERTIFICATE_INFO		 1
#define AC_OBJ_VERSION				 2
#define AC_OBJ_HOLDER_ISSUER		 5
#define AC_OBJ_HOLDER_SERIAL		 6
#define AC_OBJ_ENTITY_NAME			10
#define AC_OBJ_ISSUER_NAME			19
#define AC_OBJ_ISSUER				23
#define AC_OBJ_SIG_ALG				35
#define AC_OBJ_SERIAL_NUMBER		36
#define AC_OBJ_NOT_BEFORE			38
#define AC_OBJ_NOT_AFTER			39
#define AC_OBJ_ATTRIBUTE_TYPE		42
#define AC_OBJ_ATTRIBUTE_VALUE		44
#define AC_OBJ_EXTN_ID				49
#define AC_OBJ_CRITICAL				50
#define AC_OBJ_EXTN_VALUE			51
#define AC_OBJ_ALGORITHM			53
#define AC_OBJ_SIGNATURE			54
#define AC_OBJ_ROOF					55

/**
 * Implements x509ac_t.is_valid
 */
static err_t is_valid(const private_x509ac_t *this, time_t *until)
{
	time_t current_time = time(NULL);
	
	DBG2("  not before  : %T", &this->notBefore);
	DBG2("  current time: %T", &current_time);
	DBG2("  not after   : %T", &this->notAfter);

	if (until != NULL &&
		(*until == UNDEFINED_TIME || this->notAfter < *until))
	{
		*until = this->notAfter;
	}
	if (current_time < this->notBefore)
	{
		return "is not valid yet";
	}
	if (current_time > this->notAfter)
	{
		return "has expired";
	}
	DBG2("  attribute certificate is valid");
	return NULL;
}

/**
 * parses a directoryName
 */
static bool parse_directoryName(chunk_t blob, int level, bool implicit, identification_t **name)
{
	bool has_directoryName;
	linked_list_t *list = linked_list_create();

	parse_generalNames(blob, level, implicit, list);
	has_directoryName = list->get_count(list) > 0;

	if (has_directoryName)
	{
		iterator_t *iterator = list->create_iterator(list, TRUE);
		identification_t *directoryName;
		bool first = TRUE;

		while (iterator->iterate(iterator, (void**)&directoryName))
		{
			if (first)
			{
				*name = directoryName;
				first = FALSE;
			}
			else
			{
				DBG1("more than one directory name - first selected");
				directoryName->destroy(directoryName);
			}
		}
		iterator->destroy(iterator);
	}
	else
	{
		DBG1("no directoryName found");
	}

	list->destroy(list);
	return has_directoryName;
}

/**
 * parses ietfAttrSyntax
 */
static void parse_ietfAttrSyntax(chunk_t blob, int level0, linked_list_t *list)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;

	asn1_init(&ctx, blob, level0, FALSE, FALSE);

	while (objectID < IETF_ATTR_ROOF)
	{
		if (!extract_object(ietfAttrSyntaxObjects, &objectID, &object, &level, &ctx))
		{
			return;
		}

		switch (objectID)
		{
			case IETF_ATTR_OCTETS:
			case IETF_ATTR_OID:
			case IETF_ATTR_STRING:
				{
					ietfAttribute_t kind = (objectID - IETF_ATTR_OCTETS) / 2;
					ietfAttr_t *attr   = ietfAttr_create(kind, object);
					list->insert_last(list, (void *)attr);
				}
				break;
			default:
				break;
		}
		objectID++;
	}
}

/**
 * parses roleSyntax
 */
static void parse_roleSyntax(chunk_t blob, int level0)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;

	asn1_init(&ctx, blob, level0, FALSE, FALSE);
	while (objectID < ROLE_ROOF)
	{
		if (!extract_object(roleSyntaxObjects, &objectID, &object, &level, &ctx))
		{
			return;
		}

		switch (objectID)
		{
			default:
				break;
		}
		objectID++;
	}
}

/**
 * Parses an X.509 attribute certificate
 */
static bool parse_certificate(chunk_t blob, private_x509ac_t *this)
{
	asn1_ctx_t ctx;
	bool critical;
	chunk_t object;
	u_int level;
	u_int type = OID_UNKNOWN;
	u_int extn_oid = OID_UNKNOWN;
	int objectID = 0;

	asn1_init(&ctx, blob, 0, FALSE, FALSE);
	while (objectID < AC_OBJ_ROOF)
	{
		if (!extract_object(acObjects, &objectID, &object, &level, &ctx))
		{
			return FALSE;
		}

		/* those objects which will parsed further need the next higher level */
		level++;

		switch (objectID)
		{
			case AC_OBJ_CERTIFICATE:
				this->certificate = object;
				break;
			case AC_OBJ_CERTIFICATE_INFO:
				this->certificateInfo = object;
				break;
			case AC_OBJ_VERSION:
				this->version = (object.len) ? (1 + (u_int)*object.ptr) : 1;
				DBG2("  v%d", this->version);
				if (this->version != 2)
				{
					DBG1("v%d attribute certificates are not supported", this->version);
					return FALSE;
				}
				break;
			case AC_OBJ_HOLDER_ISSUER:
				if (!parse_directoryName(object, level, FALSE, &this->holderIssuer))
				{
					return FALSE;
				}
				break;
			case AC_OBJ_HOLDER_SERIAL:
				this->holderSerial = object;
				break;
			case AC_OBJ_ENTITY_NAME:
				if (!parse_directoryName(object, level, TRUE, &this->entityName))
				{
					return FALSE;
				}
				break;
			case AC_OBJ_ISSUER_NAME:
				if (!parse_directoryName(object, level, FALSE, &this->issuerName))
				{
					return FALSE;
				}
				break;
			case AC_OBJ_SIG_ALG:
				this->sigAlg = parse_algorithmIdentifier(object, level, NULL);
				break;
			case AC_OBJ_SERIAL_NUMBER:
				this->serialNumber = object;
				break;
			case AC_OBJ_NOT_BEFORE:
				this->notBefore = asn1totime(&object, ASN1_GENERALIZEDTIME);
				break;
			case AC_OBJ_NOT_AFTER:
				this->notAfter = asn1totime(&object, ASN1_GENERALIZEDTIME);
				break;
			case AC_OBJ_ATTRIBUTE_TYPE:
				type = known_oid(object);
				break;
			case AC_OBJ_ATTRIBUTE_VALUE:
				{
					switch (type)
					{
						case OID_AUTHENTICATION_INFO:
							DBG2("  need to parse authenticationInfo");
							break;
						case OID_ACCESS_IDENTITY:
							DBG2("  need to parse accessIdentity");
							break;
						case OID_CHARGING_IDENTITY:
							parse_ietfAttrSyntax(object, level, this->charging);
							break;
						case OID_GROUP:
							parse_ietfAttrSyntax(object, level, this->groups);
							break;
						case OID_ROLE:
							parse_roleSyntax(object, level);
							break;
						default:
							break;
					}
				}
				break;
			case AC_OBJ_EXTN_ID:
				extn_oid = known_oid(object);
				break;
			case AC_OBJ_CRITICAL:
				critical = object.len && *object.ptr;
				DBG2("  %s",(critical)?"TRUE":"FALSE");
				break;
			case AC_OBJ_EXTN_VALUE:
				{
					switch (extn_oid)
					{
						case OID_CRL_DISTRIBUTION_POINTS:
							DBG2("  need to parse crlDistributionPoints");
							break;
						case OID_AUTHORITY_KEY_ID:
							parse_authorityKeyIdentifier(object, level,
									&this->authKeyID, &this->authKeySerialNumber);
							break;
						case OID_TARGET_INFORMATION:
							DBG2("  need to parse targetInformation");
							break;
						case OID_NO_REV_AVAIL:
							this->noRevAvail = TRUE;
							break;
						default:
							break;
					}
				}
				break;
			case AC_OBJ_ALGORITHM:
				this->algorithm = parse_algorithmIdentifier(object, level, NULL);
				break;
			case AC_OBJ_SIGNATURE:
				this->signature = object;
				break;
			default:
				break;
		}
		objectID++;
	}
	this->installed = time(NULL);
	return FALSE;
}

/**
 * Implements x509ac_t.destroy
 */
static void destroy(private_x509ac_t *this)
{
	DESTROY_IF(this->holderIssuer);
	DESTROY_IF(this->entityName);
	DESTROY_IF(this->issuerName);
	this->charging->destroy_offset(this->charging, 
							offsetof(ietfAttr_t, destroy));
	this->groups->destroy_offset(this->groups,
						  offsetof(ietfAttr_t, destroy));
	free(this->certificate.ptr);
	free(this);
}

/**
 * Described in header.
 */
x509ac_t *x509ac_create_from_chunk(chunk_t chunk)
{
	private_x509ac_t *this = malloc_thing(private_x509ac_t);
	
	/* initialize */
	this->holderIssuer = NULL;
	this->entityName = NULL;
	this->issuerName = NULL;
	this->charging = linked_list_create();
	this->groups = linked_list_create();

	/* public functions */
	this->public.is_valid = (err_t (*) (const x509ac_t*,time_t*))is_valid;
	this->public.destroy = (void (*) (x509ac_t*))destroy;

	if (!parse_certificate(chunk, this))
	{
		destroy(this);
		return NULL;
	}
	return &this->public;
}

/**
 * Described in header.
 */
x509ac_t *x509ac_create_from_file(const char *filename)
{
	bool pgp = FALSE;
	chunk_t chunk = chunk_empty;

	if (!pem_asn1_load_file(filename, NULL, "attribute certificate", &chunk, &pgp))
	{
		return NULL;
	}
	return x509ac_create_from_chunk(chunk);
}

