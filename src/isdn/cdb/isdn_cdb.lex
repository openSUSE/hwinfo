WSP	[ \t]
VALCHAR [^\|]

%START Main NextLine NewEntry Value

%{
#include "isdn_cdb_def.h"
%}
%%
	int	item = 0;

<Main>{
#			BEGIN NextLine;
^\|			{
				if (new_entry())
					exit(99);
				BEGIN NewEntry;
			}
^{WSP}+			;
\n			;
}

<NextLine>{
.*			;
\n			BEGIN Main;
}

<NewEntry>{
vendor=			{item=vendor; BEGIN Value;}
device=			{item=device; BEGIN Value;}
vendor_id=		{item=vendor_id; BEGIN Value;}
device_id=		{item=device_id; BEGIN Value;}
subvendor_id=		{item=subvendor_id; BEGIN Value;}
subdevice_id=		{item=subdevice_id; BEGIN Value;}
device_class=		{item=device_class; BEGIN Value;}
bus_type=		{item=bus_type; BEGIN Value;}
vario=			{item=vario; BEGIN Value;}
SMP=			{item=SMP; BEGIN Value;}
drv_id=			{item=drv_id; BEGIN Value;}
drv_subtyp=		{item=drv_subtyp; BEGIN Value;}
drv_typ=		{item=drv_typ; BEGIN Value;}
[iI]nterface=		{item=interface; BEGIN Value;}
line_cnt=		{item=line_cnt; BEGIN Value;}
line_protocol=		{item=line_protocol; BEGIN Value;}
module=			{item=module; BEGIN Value;}
need_packages=		{item=need_packages; BEGIN Value;}
supported=		{item=supported; BEGIN Value;}
feature=		{item=feature; BEGIN Value;}
info=			{item=info; BEGIN Value;}
special=		{item=special; BEGIN Value;}
firmware=		{item=firmware; BEGIN Value;}
short_description=	{item=short_description; BEGIN Value;}
IRQ=			{item=IRQ; BEGIN Value;}
IO=			{item=IO; BEGIN Value;}
MEMBASE=		{item=MEMBASE; BEGIN Value;}
alternative_name=	{item=alternative_name; BEGIN Value;}
revision=		{item=revision; BEGIN Value;}
\n			BEGIN Main;
}

<Value>{
\|			BEGIN NewEntry;
{VALCHAR}*/\|		add_current_item(item, yytext);
}
