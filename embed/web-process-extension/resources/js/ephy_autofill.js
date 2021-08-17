'use strict';

var EphyAutofill = {
  MAX_ELEMENT_KEY_LENGTH: 64,
  MAX_FORM_LENGTH: 128,
  MAX_SELECT_ELEMENT_LENGTH: 256,
  // NOTE: @Field here must be synchronized with the C part in
  // `ephy_autofill_field.h`.
  Field: (() => {
    const Unknown = 0;

    const Firstname = 1 << 0;
    const Lastname = 1 << 1;
    const Fullname = 1 << 2;
    const Username = 1 << 3;
    const Email = 1 << 4;
    const Phone = 1 << 5;

    const StreetAddress = 1 << 6;
    const CountryCode = 1 << 7;
    const CountryName = 1 << 8;
    const Organization = 1 << 9;
    const PostalCode = 1 << 10;
    const Country = 1 << 11;
    const State = 1 << 12;
    const City = 1 << 13;

    const CardExpdateMonthMm = 1 << 14;
    const CardExpdateMonthM = 1 << 15;
    const CardExpdateMonth = 1 << 16;

    const CardExpdateYearYyyy = 1 << 17;
    const CardExpdateYearYy = 1 << 18;
    const CardExpdateYear = 1 << 19;

    const CardExpdate = 1 << 20;
    const NameOnCard = 1 << 21;
    const CardNumber = 1 << 22;

    const CardTypeCode = 1 << 23;
    const CardTypeName = 1 << 24;
    const CardType = 1 << 25;

    const Personal =
      Firstname |
      Lastname |
      Fullname |
      Username |
      Email |
      Phone |
      StreetAddress |
      CountryCode |
      CountryName |
      Organization |
      PostalCode |
      Country |
      State |
      City;

    const Card =
      CardExpdateMonthMm |
      CardExpdateMonthM |
      CardExpdateMonth |
      CardExpdateYearYyyy |
      CardExpdateYearYy |
      CardExpdateYear |
      CardExpdate |
      NameOnCard |
      CardNumber |
      CardTypeCode |
      CardTypeName |
      CardType;

    const General = CardExpdateMonth | CardExpdateYear | CardType | Country;

    const Specific = ~General;

    return {
      Unknown,

      Firstname,
      Lastname,
      Fullname,
      Username,
      Email,
      Phone,

      StreetAddress,
      CountryCode,
      CountryName,
      Organization,
      PostalCode,
      Country,
      State,
      City,

      CardExpdateMonthMm,
      CardExpdateMonthM,
      CardExpdateMonth,

      CardExpdateYearYyyy,
      CardExpdateYearYy,
      CardExpdateYear,

      CardExpdate,
      NameOnCard,
      CardNumber,

      CardTypeCode,
      CardTypeName,
      CardType,

      Personal,
      Card,

      General,
      Specific
    };
  })(),
  // NOTE: @FillChoice here must be synchronized with the C part in
  // `ephy_autofill_fill_choice.h`.
  FillChoice: {
    FormPersonal: 0,
    FormAll: 1,
    Element: 2
  },
  bootstrap(_event) {
    if (window.location.protocol !== 'https:') {
      return;
    }

    // NOTE: We can't use "click" event here, since we want to only work on
    // focused elements that are clicked, and when the element receives the
    // "click" event it's always focused. In addition, we can't use "focus" and
    // "click" events, since "focus" is always followed by a "click" event.
    document.addEventListener(
      'mousedown',
      EphyAutofill.onMousedown,
      true
    );
  },
  dispatchChangeEvent(element) {
    const eventType = 'HTMLEvents';
    const eventName = 'change';
    const bubbles = false;
    const cancellable = true;

    const event = document.createEvent(eventType);
    event.initEvent(eventName, bubbles, cancellable);

    element.dispatchEvent(event);
  },
  fillForm(form, { personal = false, creditCard = false }) {
    if (!EphyAutofill.isValidForm(form)) {
      return;
    }

    const fillOptions = { personal, creditCard };
    Array.prototype.forEach.call(form.elements, element => {
      switch (element.tagName.toLowerCase()) {
        case 'input':
          EphyAutofill.fillInput(element, fillOptions);
          break;
        case 'select':
          EphyAutofill.fillSelect(element, fillOptions);
          break;
      }
    });
  },
  fillInput(inputElement, { personal, creditCard }) {
    const value = inputElement.value;

    if (value) {
      return;
    }

    const field = EphyAutofill.getInputField(inputElement, {
      personal,
      creditCard
    });

    if (field === EphyAutofill.Field.Unknown) {
      return;
    }

    EphyAutofill.getFieldValue(field, EphyAutofill.pageId, value => {
      if (!value) {
        return;
      }

      EphyAutofill.changeValue(inputElement, value);
    });
  },
  fillSelect(selectElement, { personal, creditCard }) {
    const field = EphyAutofill.getSelectField(selectElement, {
      personal,
      creditCard
    });

    if (field === EphyAutofill.Field.Unknown) {
      return;
    }

    EphyAutofill.getFieldValue(field, EphyAutofill.pageId, value => {
      if (!value) {
        return;
      }

      const index = EphyAutofill.getSelectNamedIndex(
        selectElement,
        value.toLowerCase()
      );

      if (!(index >= 0)) {
        return;
      }

      selectElement.selectedIndex = index;
      EphyAutofill.dispatchChangeEvent(selectElement);
    });
  },
  getElementLabel(element) {
    const id = element.id;

    if (!id) {
      return null;
    }

    const labelSelector = `label[for="${id}"]`;
    const labelElement = document.querySelector(labelSelector);

    if (!labelElement) {
      return null;
    }

    const labelText = labelElement.innerText;
    return labelText;
  },
  getInputField(inputElement, { personal, creditCard }) {
    const type = (inputElement.type || '').toLowerCase();
    const isValidType =
      !type || ['email', 'search', 'tel', 'text'].includes(type);

    const isVisible = EphyAutofill.isVisible(inputElement);

    if (!(isValidType && isVisible)) {
      return EphyAutofill.Field.Unknown;
    }

    const id = inputElement.id;
    const label = EphyAutofill.getElementLabel(inputElement);
    const name = inputElement.name;

    const keys = [name, id, label];

    const fillOptions = { personal, creditCard };
    let field = EphyAutofill.Field.Unknown;
    for (const key of keys) {
      field = EphyAutofill.getInputFieldHelper(key, fillOptions);

      if (field !== EphyAutofill.Field.Unknown) {
        break;
      }
    }

    if (
      personal &&
      field === EphyAutofill.Field.Unknown &&
      type === 'email'
    ) {
      return EphyAutofill.Field.Email;
    } else if (
      personal &&
      field === EphyAutofill.Field.Unknown &&
      type === 'tel'
    ) {
      return EphyAutofill.Field.Phone;
    } else {
      return field;
    }
  },
  getInputFieldHelper(key, { personal, creditCard }) {
    if (!EphyAutofill.isValidElementKey(key)) {
      return EphyAutofill.Field.Unknown;
    }

    let field = EphyAutofill.Field.Unknown;

    if (personal) {
      field = EphyAutofill.getInputPersonalField(key);
    }
    if (creditCard && field === EphyAutofill.Field.Unknown) {
      field = EphyAutofill.getInputCreditCardField(key);
    }

    return field;
  },
  getInputPersonalField(key) {
    const steps = [
      {
        matcher: EphyAutofill.matchers.isFullName,
        field: EphyAutofill.Field.Fullname
      },
      {
        matcher: EphyAutofill.matchers.isFirstName,
        field: EphyAutofill.Field.Firstname
      },
      {
        matcher: EphyAutofill.matchers.isLastName,
        field: EphyAutofill.Field.Lastname
      },
      {
        matcher: EphyAutofill.matchers.isUsername,
        field: EphyAutofill.Field.Username
      },
      {
        matcher: EphyAutofill.matchers.isEmail,
        field: EphyAutofill.Field.Email
      },
      {
        matcher: EphyAutofill.matchers.isPhone,
        field: EphyAutofill.Field.Phone
      },
      {
        matcher: EphyAutofill.matchers.isOrganization,
        field: EphyAutofill.Field.Organization
      },
      {
        matcher: EphyAutofill.matchers.isPostalCode,
        field: EphyAutofill.Field.PostalCode
      },
      {
        matcher: EphyAutofill.matchers.isCountry,
        field: EphyAutofill.Field.CountryName
      },
      {
        matcher: EphyAutofill.matchers.isState,
        field: EphyAutofill.Field.State
      },
      {
        matcher: EphyAutofill.matchers.isCity,
        field: EphyAutofill.Field.City
      },
      {
        matcher: EphyAutofill.matchers.isStreetAddress,
        field: EphyAutofill.Field.StreetAddress
      }
    ];

    for (const { matcher, field: matcherField } of steps) {
      if (matcher(key)) {
        return matcherField;
      }
    }

    return EphyAutofill.Field.Unknown;
  },
  getInputCreditCardField(key) {
    if (
      EphyAutofill.matchers.isCardExpdateMonth(key) &&
      EphyAutofill.matchers.isCardExpdateYear(key)
    ) {
      if (EphyAutofill.matchers.isMonth(key)) {
        return EphyAutofill.Field.CardExpdateMonth;
      } else if (EphyAutofill.matchers.isYear(key)) {
        return EphyAutofill.Field.CardExpdateYear;
      } else {
        return EphyAutofill.Field.CardExpdate;
      }
    }

    const steps = [
      {
        matcher: EphyAutofill.matchers.isCardExpdateMonth,
        field: EphyAutofill.Field.CardExpdateMonth
      },
      {
        matcher: EphyAutofill.matchers.isCardExpdateYear,
        field: EphyAutofill.Field.CardExpdateYear
      },
      {
        matcher: EphyAutofill.matchers.isCardExpdate,
        field: EphyAutofill.Field.CardExpdate
      },
      {
        matcher: EphyAutofill.matchers.isNameOnCard,
        field: EphyAutofill.Field.NameOnCard
      },
      {
        matcher: EphyAutofill.matchers.isCardNumber,
        field: EphyAutofill.Field.CardNumber
      },
      {
        matcher: EphyAutofill.matchers.isCardType,
        field: EphyAutofill.Field.CardType
      }
    ];

    for (const { matcher, field: matcherField } of steps) {
      if (matcher(key)) {
        return matcherField;
      }
    }

    return EphyAutofill.Field.Unknown;
  },
  getFormField(formElement, { personal, creditCard }) {
    if (!EphyAutofill.isValidForm(formElement)) {
      return EphyAutofill.Field.Unknown;
    }

    const fillOptions = { personal, creditCard };
    let field = EphyAutofill.Field.Unknown;
    Array.prototype.forEach.call(formElement.elements, element => {
      switch (element.tagName.toLowerCase()) {
        case 'input':
          field |= EphyAutofill.getInputField(element, fillOptions);
          break;
        case 'select':
          field |= EphyAutofill.getSelectField(element, fillOptions);
          break;
      }
    });

    return field;
  },
  getSelectField(selectElement, { personal, creditCard }) {
    const isVisible = EphyAutofill.isVisible(selectElement);

    if (!isVisible) {
      return EphyAutofill.Field.Unknown;
    }

    const id = selectElement.id;
    const label = EphyAutofill.getElementLabel(selectElement);
    const name = selectElement.name;

    const keys = [name, id, label];

    const fillOptions = { personal, creditCard };
    let field = EphyAutofill.Field.Unknown;
    for (const key of keys) {
      field = EphyAutofill.getSelectFieldHelper(key, fillOptions);

      if (field !== EphyAutofill.Field.Unknown) {
        return field;
      }
    }

    return EphyAutofill.Field.Unknown;
  },
  getSelectFieldHelper(key, { personal, creditCard }) {
    if (!EphyAutofill.isValidElementKey(key)) {
      return EphyAutofill.Field.Unknown;
    }

    let field = EphyAutofill.Field.Unknown;

    if (personal) {
      field = EphyAutofill.getSelectPersonalField(key);
    }
    if (creditCard && field === EphyAutofill.Field.Unknown) {
      field = EphyAutofill.getSelectCreditCardField(key);
    }

    return field;
  },
  getSelector(element) {
    const path = [];

    for (let el = element; el; el = el.parentNode) {
      if (el.tagName.toLowerCase() === 'body') {
        break;
      } else if (el.id) {
        path.unshift(`#${el.id}`);
        break;
      } else {
        path.unshift(getNthSelector(el));
      }
    }

    return path.join(' > ');

    function getNthSelector (element) {
      const elementTagName = element.tagName.toLowerCase();
      let previousSiblings = 0;

      for (
        let sibling = element.previousSibling;
        sibling;
        sibling = sibling.previousSibling
      ) {
        if (sibling.nodeName.toLowerCase() === elementTagName) {
          previousSiblings++;
        }
      }

      return `${elementTagName}:nth-of-type(${previousSiblings + 1})`;
    }
  },
  getSelectNamedIndex(selectElement, name) {
    if (
      !(
        name &&
        selectElement.length > 0 &&
        selectElement.length <= EphyAutofill.MAX_SELECT_ELEMENT_LENGTH
      )
    ) {
      return -1;
    }

    for (let i = 0; i < selectElement.length; i++) {
      const optionElement = selectElement.item(i);
      const label = (optionElement.label || '').toLowerCase();
      const text = (optionElement.text || '').toLowerCase();
      const value = (optionElement.value || '').toLowerCase();

      if (label === name || text === name || value === name) {
        return i;
      }
    }

    return -1;
  },
  getSelectPersonalField(key) {
    const steps = [
      {
        matcher: EphyAutofill.matchers.isCountry,
        field: EphyAutofill.Field.CountryName
      },
      {
        matcher: EphyAutofill.matchers.isState,
        field: EphyAutofill.Field.State
      },
      {
        matcher: EphyAutofill.matchers.isCity,
        field: EphyAutofill.Field.City
      }
    ];

    for (const { matcher, field: matcherField } of steps) {
      if (matcher(key)) {
        return matcherField;
      }
    }

    return EphyAutofill.Field.Unknown;
  },
  getSelectCreditCardField(key) {
    if (
      EphyAutofill.matchers.isCardExpdateMonth(key) &&
      EphyAutofill.matchers.isCardExpdateYear(key)
    ) {
      if (EphyAutofill.matchers.isMonth(key)) {
        return EphyAutofill.Field.CardExpdateMonth;
      } else if (EphyAutofill.matchers.isYear(key)) {
        return EphyAutofill.Field.CardExpdateYear;
      } else {
        return EphyAutofill.Field.Unknown;
      }
    }

    const steps = [
      {
        matcher: EphyAutofill.matchers.isCardExpdateMonth,
        field: EphyAutofill.Field.CardExpdateMonth
      },
      {
        matcher: EphyAutofill.matchers.isCardExpdateYear,
        field: EphyAutofill.Field.CardExpdateYear
      },
      {
        matcher: EphyAutofill.matchers.isCardType,
        field: EphyAutofill.Field.CardType
      }
    ];

    for (const { matcher, field: matcherField } of steps) {
      if (matcher(key)) {
        return matcherField;
      }
    }

    return EphyAutofill.Field.Unknown;
  },
  isValidElementKey(key) {
    return (
      key &&
      key.length > 0 &&
      key.length < EphyAutofill.MAX_ELEMENT_KEY_LENGTH
    );
  },
  isValidForm(form) {
    return (
      form &&
      form.tagName.toLowerCase() === 'form' &&
      form.length > 0 &&
      form.length < EphyAutofill.MAX_FORM_LENGTH
    );
  },
  isVisible(element) {
    if (
      !(
        element &&
        element.tagName &&
        ['input', 'select'].includes(element.tagName.toLowerCase())
      )
    ) {
      return false;
    }

    const { width, height } = element.getBoundingClientRect();

    return width > 1 && height > 1;
  },
  matchers: (() => {
    const factory = regexes => {
      const unifiedRegex = new RegExp(
        regexes.map(regex => `(${regex})`).join('|'),
        'i'
      );

      return key => {
        return !!key.match(unifiedRegex);
      };
    };

    return {
      isFirstName: factory([
        'first.*name',
        'initials',
        'fname',
        'first$',
        'given.*name'
      ]),
      isLastName: factory([
        'last.*name',
        'lname',
        'surname',
        'last$',
        'secondname',
        'family.*name'
      ]),
      isFullName: factory([
        '^name',
        'full.?name',
        'your.?name',
        'customer.?name',
        'bill.?name',
        'ship.?name',
        'name.*first.*last',
        'firstandlastname'
      ]),
      isUsername: factory(['user.?name', 'nick.?name']),
      isEmail: factory(['e.?mail']),
      isPhone: factory(['phone', 'mobile']),

      isStreetAddress: factory([
        'address.*line',
        'address1',
        'addr1',
        'street',
        '(shipping|billing)address$',
        'house.?name',
        'address',
        'line'
      ]),
      isOrganization: factory([
        'company',
        'business',
        'organization',
        'organisation'
      ]),
      isPostalCode: factory([
        'zip',
        'postal',
        'post.*code',
        'pcode',
        'pin.?code'
      ]),
      isCountry: factory(['country', 'countries', 'location']),
      isState: factory([
        'province',
        'region',
        'state',
        'county',
        'region',
        'province',
        'county',
        'principality'
      ]),
      isCity: factory(['city', 'town', 'suburb']),

      isCardExpdateMonth: factory([
        'expir',
        'exp.*mo',
        'exp.*date',
        'ccmonth',
        'cardmonth'
      ]),
      isCardExpdateYear: factory(['exp', '^/', 'year']),
      isCardExpdate: factory(['expir', 'exp.*date']),
      isNameOnCard: factory([
        'card.?(holder|owner)',
        'name.*\\bon\\b.*card',
        '(card|cc).?name',
        'cc.?full.?name'
      ]),
      isCardNumber: factory(['(card|cc|acct).?(number|#|no|num)']),
      isCardType: factory(['debit.*card', '(card|cc).?type']),

      isMonth: factory(['month']),
      isYear: factory(['year'])
    };
  })(),
  onMousedown(event) {
    const element = event.target;

    if (element.tagName.toLowerCase() !== 'input') {
      return false;
    }

    if (element.value) {
      return false;
    }

    const fieldOptions = { personal: true, creditCard: true };
    const formField = EphyAutofill.getFormField(
      element.form,
      fieldOptions
    );
    const isFillableElement =
      EphyAutofill.getInputField(element, fieldOptions) !==
      EphyAutofill.Field.Unknown;
    const hasPersonalFields =
      (formField & EphyAutofill.Field.Personal) !== 0;
    const hasCardFields = (formField & EphyAutofill.Field.Card) !== 0;

    if (!(isFillableElement || hasPersonalFields || hasCardFields)) {
      return false;
    }

    const selector = EphyAutofill.getSelector(element);
    const { x, y, width, height } = element.getBoundingClientRect();

    const pageId = EphyAutofill.pageId;

    window.webkit.messageHandlers.autofillAskUser.postMessage({
            pageId, formField, selector, isFillableElement, hasPersonalFields, hasCardFields, x, y, width, height});

    return false;
  },

  fill(pageId, selector, fillChoice) {
    if (pageId !== EphyAutofill.pageId) {
      return;
    }

    const isValidFillChoice = fillChoice >= 0 && fillChoice <= EphyAutofill.FillChoice.Element;
    const isValidSelector = !!selector;

    if (!(isValidFillChoice && isValidSelector)) {
      return;
    }

    const element = document.querySelector(selector);

    if (element.tagName.toLowerCase() !== 'input') {
      return;
    }

    const form = element.form;

    switch (fillChoice) {
      case EphyAutofill.FillChoice.FormPersonal:
        EphyAutofill.fillForm(form, {
          personal: true,
          creditCard: false
        });
        break;
      case EphyAutofill.FillChoice.FormAll:
        EphyAutofill.fillForm(form, {
          personal: true,
          creditCard: true
        });
        break;
      case EphyAutofill.FillChoice.Element:
        EphyAutofill.fillInput(element, {
          personal: true,
          creditCard: true
        });
        break;
    }
  }

};

if (document.readyState === 'complete') {
  EphyAutofill.bootstrap();
} else {
  window.addEventListener('load', EphyAutofill.bootstrap, true);
}
